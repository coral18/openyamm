# Nextcloud release downloads

GitHub Actions builds the usual complete Windows ZIP, Linux Flatpak and signed Android APK. Scheduled builds,
tagged releases and manual runs with publishing enabled upload them and their SHA256 files to Nextcloud.
Each file remains a normal download; users do not join pieces or install separate game content.
GitHub releases contain checksums and direct Nextcloud download links, avoiding GitHub's 2 GiB release-asset limit.
Ordinary pushes and manual runs with publishing disabled retain packages as one-day Actions artifacts.

## Configure cloud.jasicek.net

1. Create the `openyamm` folder with `nightly` and `releases` subfolders.
2. In Nextcloud's personal settings, open **Security** and create an app password named `GitHub Actions`.
3. In the GitHub repository, open **Settings → Secrets and variables → Actions**. Add repository secrets:
   - `OPENYAMM_NEXTCLOUD_USERNAME`: the account's login username, as used in its WebDAV URL, not its display name.
   - `OPENYAMM_NEXTCLOUD_APP_PASSWORD`: the generated app password.
4. Share the **openyamm folder itself** with a public read-only link, allowing downloads. Copy the `/s/…` URL.
5. On GitHub's **Variables** tab, add:
   - `OPENYAMM_NEXTCLOUD_FOLDER`: `openyamm`.
   - `OPENYAMM_NEXTCLOUD_PUBLIC_URL`: the copied share URL, such as `https://cloud.jasicek.net/s/your-token`.

The server defaults to `https://cloud.jasicek.net`. Set the optional repository variable
`OPENYAMM_NEXTCLOUD_URL` to change it. The uploader uses HTTPS and does not follow redirects with credentials.
The public share must point to the folder configured by `OPENYAMM_NEXTCLOUD_FOLDER` and permit downloading
without a password. Credentials belong in GitHub Secrets; the public URL belongs in Variables.
If the public URL is omitted, builds still upload privately to Nextcloud and skip GitHub release publication.

## Paths and verification

Nightlies go into `openyamm/nightly/<run-id>-<attempt>/`. Tags go into
`openyamm/releases/<version>/<run-id>-<attempt>/`. Each attempt has its own directory, keeping published links
and downloads intact while another build uploads. After a nightly upload and GitHub release update succeed,
the workflow removes previous nightly run directories, retaining only the newly published build. A failed upload
or publication keeps the previous nightly available. Tagged releases are retained and never pruned or overwritten.
Deleted nightlies may remain in Nextcloud's Trash until its
[trash retention policy](https://docs.nextcloud.com/server/24/admin_manual/configuration_files/trashbin_configuration.html)
expires them; removing their public directories does not bypass that server policy.

The uploader sends 64 MiB chunks using the
[Nextcloud chunked WebDAV API](https://docs.nextcloud.com/server/24/developer_manual/client_apis/WebDAV/chunking.html),
then asks the server to assemble each file and verifies its reported size. Checksums are verified before uploading
and published alongside each package. Temporary incomplete uploads expire on the server after 24 hours.

Allow at least 64 MiB HTTP request bodies through any proxy and enough time for the final assembly request.
The account needs roughly 11 GB per complete build, plus temporary space for assembling the largest file.
See [Nextcloud large upload configuration](https://docs.nextcloud.com/server/24/admin_manual/configuration_files/big_file_upload_configuration.html)
for web server timeouts, temporary storage and download proxy buffering.

Run the uploader's local protocol checks with:

```sh
python3 packaging/test_upload_nextcloud.py
```

After configuring access, run **Actions → Package builds → Run workflow**, leaving publishing enabled.
The upload step must pass before any GitHub release is updated. Its job summary contains the public download links.
Actual server access and a multi-gigabyte upload require this first configured run; local checks do not validate
the account's permissions, quota or proxy settings.
