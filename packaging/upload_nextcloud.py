#!/usr/bin/env python3
"""Upload complete release files through Nextcloud's chunked WebDAV API."""

import argparse
import base64
import http.client
import os
from pathlib import Path, PurePosixPath
import re
import time
from urllib.parse import quote, unquote, urlencode, urlsplit
import uuid
import xml.etree.ElementTree as ET


CHUNK_SIZE = 64 * 1024 * 1024


def request(method, url, authorization, data=None, headers=None, accepted=(200, 201, 204), read_body=False):
    headers = {"Authorization": authorization, **(headers or {})}
    parsed = urlsplit(url)
    for attempt in range(4):
        connection = http.client.HTTPSConnection(parsed.hostname, parsed.port, timeout=600)
        try:
            connection.request(method, parsed.path, body=data, headers=headers)
            response = connection.getresponse()
            if response.status in accepted:
                return response.read() if read_body else response.headers
            if response.status not in (429, 502, 503, 504) or attempt == 3:
                raise RuntimeError(f"Nextcloud {method} failed: HTTP {response.status}")
        except (OSError, http.client.HTTPException):
            if attempt == 3:
                raise RuntimeError(f"Nextcloud {method} failed: connection interrupted or timed out") from None
        finally:
            connection.close()
        time.sleep(2 ** attempt)


def upload_file(path, destination, uploads, authorization):
    size = path.stat().st_size
    if not size:
        raise ValueError(f"Cannot publish an empty file: {path.name}")
    upload = uploads + "/openyamm-" + uuid.uuid4().hex
    request("MKCOL", upload, authorization)
    headers = {"OC-Total-Length": str(size)}
    with path.open("rb") as stream:
        part = 1
        while chunk := stream.read(CHUNK_SIZE):
            request("PUT", upload + f"/{part:05d}", authorization, chunk, headers)
            print(f"{path.name}: uploaded {min(part * CHUNK_SIZE, size):,}/{size:,} bytes", flush=True)
            part += 1
    # The server assembles the chunks into the original APK, ZIP or Flatpak.
    request("MOVE", upload + "/.file", authorization, headers={"Destination": destination, **headers})
    result = request("HEAD", destination, authorization)
    if int(result.get("Content-Length", -1)) != size:
        raise RuntimeError(f"Nextcloud uploaded file size differs: {path.name}")


def relative_path(value):
    path = PurePosixPath(value)
    if not path.parts or path.is_absolute() or ".." in path.parts or "\\" in value:
        raise ValueError("Nextcloud folders must be nonempty relative paths without '..'")
    return path.as_posix()


def prune_nightlies(nightly, keep, authorization):
    listing = request("PROPFIND", nightly + "/", authorization,
                      data=b'<d:propfind xmlns:d="DAV:"><d:prop><d:resourcetype/></d:prop></d:propfind>',
                      headers={"Depth": "1", "Content-Type": "application/xml"}, accepted=(207,), read_body=True)
    prefix = unquote(urlsplit(nightly).path).rstrip("/") + "/"
    names = set()
    for response in ET.fromstring(listing).findall("{DAV:}response"):
        path = unquote(urlsplit(response.findtext("{DAV:}href", "")).path).rstrip("/")
        if not path.startswith(prefix):
            continue
        name = path[len(prefix):]
        if not re.fullmatch(r"[0-9]+-[0-9]+", name):
            continue
        for properties in response.findall("{DAV:}propstat"):
            if properties.findtext("{DAV:}status", "").split()[1:2] != ["200"]:
                continue
            if properties.find("{DAV:}prop/{DAV:}resourcetype/{DAV:}collection") is not None:
                names.add(name)
    if keep not in names:
        raise RuntimeError("Current nightly folder is missing; previous nightlies were not removed")
    for name in sorted(names - {keep}):
        request("DELETE", nightly + "/" + name, authorization, accepted=(204, 404))
        print(f"Removed previous nightly: {name}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path, nargs="?")
    parser.add_argument("--remote-path", required=True, help="Build directory below the configured shared folder")
    parser.add_argument("--links-file", type=Path)
    parser.add_argument("--prune-nightlies", action="store_true",
                        help="Keep the published run and remove older nightlies")
    args = parser.parse_args()
    server = os.environ.get("OPENYAMM_NEXTCLOUD_URL", "https://cloud.jasicek.net").rstrip("/")
    username = os.environ.get("OPENYAMM_NEXTCLOUD_USERNAME", "")
    password = os.environ.get("OPENYAMM_NEXTCLOUD_APP_PASSWORD", "")
    folder = relative_path(os.environ.get("OPENYAMM_NEXTCLOUD_FOLDER", "openyamm"))
    remote = relative_path(args.remote_path)
    public = os.environ.get("OPENYAMM_NEXTCLOUD_PUBLIC_URL", "").rstrip("/")
    for url in (server, public) if public else (server,):
        parsed = urlsplit(url)
        if parsed.scheme != "https" or not parsed.hostname or parsed.username or parsed.query or parsed.fragment:
            raise ValueError("Nextcloud URLs must be HTTPS URLs without credentials, query or fragment")
    if not username or not password:
        raise ValueError("Configure OPENYAMM_NEXTCLOUD_USERNAME and OPENYAMM_NEXTCLOUD_APP_PASSWORD secrets")
    authorization = "Basic " + base64.b64encode(f"{username}:{password}".encode()).decode()
    dav = server + "/remote.php/dav/"
    root = dav + "files/" + quote(username, safe="") + "/" + quote(folder, safe="/")
    if args.prune_nightlies:
        if not re.fullmatch(r"nightly/[0-9]+-[0-9]+", remote):
            parser.error("Nightly cleanup requires a nightly/<run-id>-<attempt> path")
        prune_nightlies(root + "/nightly", remote.split("/")[1], authorization)
        return
    if args.directory is None or args.links_file is None:
        parser.error("Uploading requires a release directory and --links-file")
    files = sorted(args.directory.iterdir())
    if not files or any(not path.is_file() or path.is_symlink() for path in files):
        raise ValueError("Release directory must contain only regular release files")
    destination = dav + "files/" + quote(username, safe="")
    for component in (folder + "/" + remote).split("/"):
        destination += "/" + quote(component, safe="")
        request("MKCOL", destination, authorization, accepted=(201, 405))
    uploads = dav + "uploads/" + quote(username, safe="")
    for path in files:
        upload_file(path, destination + "/" + quote(path.name, safe=""), uploads, authorization)
    links = []
    if public:
        for path in files:
            url = public + "/download?" + urlencode({"path": "/" + remote, "files": path.name})
            links.append(f"- [{path.name}]({url})")
    args.links_file.write_text("\n".join(links) + "\n", encoding="utf-8")
    print(f"PASS: uploaded {len(files)} complete files to Nextcloud", flush=True)


if __name__ == "__main__":
    main()
