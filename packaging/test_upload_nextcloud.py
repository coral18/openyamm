#!/usr/bin/env python3
"""Exercise binary assembly, HTTP failures and public links without a remote account."""

from contextlib import redirect_stdout
from email.message import Message
import hashlib
import io
import os
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch
from urllib.parse import parse_qs, urlsplit

import upload_nextcloud as uploader


class UploadTest(unittest.TestCase):
    def test_upload_and_download_links(self):
        chunks = {}
        files = {}
        retried = False

        class Connection:
            def __init__(self, host, port, timeout):
                self.status = 201
                self.headers = Message()
                self.path = None

            def request(self, method, path, body, headers):
                nonlocal retried
                self.path = path
                self.headers = Message()
                if method == "MKCOL":
                    self.status = 405 if path == "/remote.php/dav/files/build%20user/openyamm" else 201
                elif method == "PUT":
                    if not retried:
                        retried = True
                        self.status = 503
                        return
                    chunks[path] = body
                elif method == "MOVE":
                    prefix = path.removesuffix(".file")
                    payload = b"".join(chunks[key] for key in sorted(chunks) if key.startswith(prefix))
                    self.assert_total(payload, headers)
                    files[urlsplit(headers["Destination"]).path] = payload
                elif method == "HEAD":
                    self.status = 200
                    self.headers["Content-Length"] = str(len(files[path]))

            @staticmethod
            def assert_total(payload, headers):
                if len(payload) != int(headers["OC-Total-Length"]):
                    raise AssertionError("Chunk assembly changed the file length")

            def getresponse(self):
                return SimpleNamespace(status=self.status, headers=self.headers)

            def close(self):
                pass

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "assets"
            source.mkdir()
            # Non-text bytes crossing several chunk boundaries must survive unchanged.
            payload = bytes(range(256)) * 4 + b"\x00\xffend"
            (source / "OpenYAMM test.apk").write_bytes(payload)
            (source / "OpenYAMM test.apk.sha256").write_text(hashlib.sha256(payload).hexdigest())
            notes = root / "links.md"
            environment = {
                "OPENYAMM_NEXTCLOUD_USERNAME": "build user",
                "OPENYAMM_NEXTCLOUD_APP_PASSWORD": "private-app-password",
                "OPENYAMM_NEXTCLOUD_PUBLIC_URL": "https://cloud.jasicek.net/s/public-token",
            }
            arguments = ["upload_nextcloud.py", str(source), "--remote-path", "nightly/42-1",
                         "--links-file", str(notes)]
            output = io.StringIO()
            with patch.dict(os.environ, environment, clear=True), patch.object(sys, "argv", arguments), \
                    patch.object(uploader.http.client, "HTTPSConnection", Connection), \
                    patch.object(uploader, "CHUNK_SIZE", 400), patch.object(uploader.time, "sleep"), \
                    redirect_stdout(output):
                uploader.main()
            self.assertTrue(retried)
            self.assertEqual(files["/remote.php/dav/files/build%20user/openyamm/nightly/42-1/OpenYAMM%20test.apk"],
                             payload)
            for line in notes.read_text().splitlines():
                link = line.split("](", 1)[1].removesuffix(")")
                query = parse_qs(urlsplit(link).query)
                self.assertEqual(query["path"], ["/nightly/42-1"])
                self.assertTrue((source / query["files"][0]).is_file())
            self.assertNotIn(environment["OPENYAMM_NEXTCLOUD_APP_PASSWORD"], output.getvalue())

    def test_rejected_uploads_do_not_retry_or_hide_failure(self):
        for status in (401, 403, 413, 507, 302):
            with self.subTest(status=status), patch.object(uploader.http.client, "HTTPSConnection") as connection:
                connection.return_value.getresponse.return_value.status = status
                with self.assertRaisesRegex(RuntimeError, f"HTTP {status}"):
                    uploader.request("PUT", "https://cloud.jasicek.net/chunk", "Basic private")
                self.assertEqual(connection.call_count, 1)

    def test_incomplete_uploaded_file_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "test.apk"
            source.write_bytes(b"release bytes")
            with patch.object(uploader, "request", return_value={"Content-Length": "1"}), \
                    redirect_stdout(io.StringIO()):
                with self.assertRaisesRegex(RuntimeError, "file size differs"):
                    uploader.upload_file(source, "https://cloud.jasicek.net/destination",
                                         "https://cloud.jasicek.net/uploads", "Basic private")

    def test_nightly_cleanup_preserves_current_and_unrelated_content(self):
        nightly = "https://cloud.jasicek.net/remote.php/dav/files/build%20user/openyamm/nightly"
        entries = [
            (nightly + "/", True),
            (nightly + "/41-1/", True),
            (nightly + "/41-2/", True),
            (nightly + "/42-1/", True),
            (nightly + "/40-1", False),
            (nightly + "/notes/", True),
            (nightly + "/40-2/child/", True),
            (nightly + "/%2e%2e/releases/", True),
            (nightly.replace("/nightly", "/releases") + "/41-1/", True),
        ]
        xml = '<d:multistatus xmlns:d="DAV:">'
        for href, directory in entries:
            resource = "<d:collection/>" if directory else ""
            xml += (f"<d:response><d:href>{href}</d:href><d:propstat><d:prop>"
                    f"<d:resourcetype>{resource}</d:resourcetype></d:prop>"
                    "<d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response>")
        xml += "</d:multistatus>"
        deleted = []

        def respond(method, url, authorization, **kwargs):
            if method == "PROPFIND":
                return xml.encode()
            self.assertEqual(method, "DELETE")
            deleted.append(url)

        with patch.object(uploader, "request", respond), redirect_stdout(io.StringIO()):
            uploader.prune_nightlies(nightly, "42-1", "Basic private")
        self.assertEqual(deleted, [nightly + "/41-1", nightly + "/41-2"])

    def test_nightly_cleanup_refuses_to_remove_last_build_if_replacement_is_missing(self):
        listing = b'<d:multistatus xmlns:d="DAV:"/>'
        with patch.object(uploader, "request", return_value=listing) as request:
            with self.assertRaisesRegex(RuntimeError, "Current nightly folder is missing"):
                uploader.prune_nightlies("https://cloud.jasicek.net/nightly", "42-1", "Basic private")
        self.assertEqual(request.call_count, 1)


if __name__ == "__main__":
    unittest.main()
