
# Notes

- Clone from https://github.com/ozanoner/embedded-devops-ch04
- Set remote repo
- remove tests and .github/workflows/ci.yml

- Update .devcontainer/devcontainer.json
- Add .env (defining WIFI_SSID and WIFI_PWD)
- Add wifi creds as secrets to the repo. Note: this prevent creds to be seen on the repo but still embedded in the firmware binary and can be extracted.
- Update .github/workflows/release.yml for wifi creds

- Update blinky/main/CMakeLists.txt to pass the WiFi SSID and password to the app from the environment
- Update blinky/main/idf_component.yml to import ozanoner/devops_easy_connect
- Update blinky/main/blinky.c to enable Wifi in the app.



- Add blinky/partitions.txt and blinky/sdkconfig.defaults


## Tags & releases

Remove existing tags and tag the current version

```bash
git tag -l # list
git tag -d v0.1.0 # delete
git tag -a v0.1.0 -m "Release version 0.1.0" # create a tag
git push origin v0.1.0 # push remote

git push && git push --tags
```

## updating to a new version
1. update version.txt
2. update code
3. build and verify
```
esptool.py --chip esp32 image_info build/blinky.bin

Application Information
=======================
Project name: blinky
App version: 0.1.1
Compile time: Aug 27 2026 16:34:00
ELF file SHA256: f71d4cc5c5157c29453f0f75b36f27d3925f3ebe144b35546b9d70906e501519
ESP-IDF: v6.0
Minimal eFuse block revision: 0.0
Maximal eFuse block revision: 0.99
MMU page size: 64 KB
Secure version: 0
```

4. commit, tag, and push


## signing

Application signing is enabled without hardware Secure Boot using the Secure
Boot version 2 RSA-3072 signing scheme. The bootloader checks the signature
when an app boots, and OTA updates are checked before they are accepted.
Hardware Secure Boot remains disabled, so this protects against unsigned or
tampered network updates but does not prevent physical bootloader replacement.
This configuration requires an ESP32 revision 3.1 or newer; the connected
devkit is revision 3.1.
It also uses a 4 MB flash image layout so the two OTA slots have room for
signed application images.

Keep the private signing key outside source control in
`../keys/signing_key.pem`, relative to the `blinky` project directory. The
repository-level `keys/` directory is ignored by Git.


```bash
root@d443f0cad006:/workspace/blinky# esptool chip_id
Warning: Deprecated: Command 'chip_id' is deprecated. Use 'chip-id' instead.
esptool v5.2.0
Connected to ESP32 on /dev/ttyUSB0:
Chip type:          ESP32-D0WD-V3 (revision v3.1)
Features:           Wi-Fi, BT, Dual Core + LP Core, 240MHz, Vref calibration in eFuse, Coding Scheme None
Crystal frequency:  40MHz
MAC:                34:5f:45:c4:f8:94

Stub flasher running.

Warning: ESP32 has no chip ID. Reading MAC address instead.
MAC:                34:5f:45:c4:f8:94

Hard resetting via RTS pin...
```

To build and verify a new version 2 image, the signing key must be an RSA-3072
key. Generate or replace the key first, then remove the stale temporary
configuration before rebuilding:

```bash
espsecure generate-signing-key --version 2 --scheme rsa3072 \
	../keys/signing_key.pem
rm -f sdkconfig sdkconfig.old
idf.py reconfigure
idf.py build
espsecure verify-signature --version 2 \
	--keyfile ../keys/signing_key.pem build/blinky.bin
```


The same signing key must be available to release builds. Do not commit the
private key or expose it in workflow logs. A release build that cannot read
`../keys/signing_key.pem` will fail intentionally.

For GitHub releases, add a repository Actions secret named `SIGNING_KEY` with
the complete contents of the RSA-3072 PEM file, including the `BEGIN` and `END`
lines. The release workflow writes this secret to `keys/signing_key.pem`, then
signs `build/blinky-unsigned.bin` with Secure Boot version 2 before publishing
`build/blinky.bin`.

## SignServer command-line method

The following is an explicit command-line method that does not use a wrapper.
It uses the local SignServer REST endpoint and the `PlainSigner` worker. The
worker must have these settings:

The ESP-IDF devcontainer is configured with Docker host networking in
`.devcontainer/devcontainer.json`. Start SignServer on the host with its
published HTTPS port `8444`, then recreate or reopen the devcontainer. From
inside the devcontainer, use `https://localhost:8444` to reach SignServer.

```text
SIGNATUREALGORITHM=NONEwithRSAandMGF1
CLIENTSIDEHASHING=true
```

Run all commands below from the `blinky` directory. Before starting, make sure
these files exist one directory above it:

```text
../tmp/signserver-keys/client.crt
../tmp/signserver-keys/client.key
../tmp/signserver-keys/ca.crt
```

The private signing key remains in SignServer. The public key is extracted from
the signer certificate returned by SignServer below.

1. Remove the temporary ESP-IDF configuration so the defaults are applied:

```bash
rm -f sdkconfig sdkconfig.old
```

2. Reconfigure and build the unsigned application:

```bash
idf.py reconfigure
idf.py build
```

The unsigned application is `build/blinky-unsigned.bin`. Confirm that the
build did not use a local private key:

```bash
grep 'espsecure sign-data' build/build.ninja
```

3. Hash the unsigned application and encode the hash for SignServer:

```bash
openssl dgst -sha256 -binary build/blinky-unsigned.bin | base64 -w0 \
	> build/blinky.sha256.b64
```

4. Send the hash to SignServer using mTLS. Replace the URL or worker name if
your local SignServer uses different values:

```bash
curl --fail-with-body --silent --show-error \
	--cert ../tmp/signserver-keys/client.crt \
	--key ../tmp/signserver-keys/client.key \
	--cacert ../tmp/signserver-keys/ca.crt \
	-H 'X-Keyfactor-Requested-With: REST' \
	-H 'Content-Type: application/json' \
	-H 'Accept: application/json' \
	--data "{\"data\":\"$(cat build/blinky.sha256.b64)\",\"encoding\":\"BASE64\",\"metaData\":{\"USING_CLIENTSUPPLIED_HASH\":\"true\",\"CLIENTSIDE_HASHDIGESTALGORITHM\":\"SHA256\"}}" \
	'https://localhost:8444/signserver/rest/v1/workers/PlainSigner/process' \
	> build/signserver-response.json
```

5. Extract the signer certificate and public key returned by SignServer, then
extract the binary signature. Signing is performed by SignServer; the commands
below only decode the JSON response:

```bash
jq -r .signerCertificate build/signserver-response.json \
	| base64 -d > build/signer-certificate.der
openssl x509 -inform DER -in build/signer-certificate.der -pubkey -noout \
	> ../tmp/signserver-keys/signing_public.pem
jq -r .data build/signserver-response.json \
	| base64 -d > build/signature.bin
```

6. Assemble the ESP32 Secure Boot v2 signature block locally using the public
key and the signature returned by SignServer:

```bash
espsecure sign-data --version 2 \
	--pub-key ../tmp/signserver-keys/signing_public.pem \
	--signature build/signature.bin \
	--output build/blinky.bin \
	build/blinky-unsigned.bin
```

7. Verify the completed signed application:

```bash
espsecure verify-signature --version 2 \
	--keyfile ../tmp/signserver-keys/signing_public.pem build/blinky.bin
```

8. Flash the bootloader, partition table, OTA data, and signed application:

```bash
idf.py -p /dev/ttyUSB0 flash
```

Never commit the mTLS private key, the SignServer private key, or the files in
`build/`. The `localhost:8444` URL works because the devcontainer uses host
networking. A runner on the Compose network must instead use
`https://signserver:8443/signserver/rest/v1/workers/PlainSigner/process`.

## Verified SignServer setup (working end to end)

This setup was verified on SignServer CE **7.3.2** + ESP-IDF 6.0. It produces a
valid ESP32 Secure Boot v2 RSA-3072 signed image where the private key is held
inside SignServer.

### Architecture

| Component | Value |
|---|---|
| Crypto token worker | `Esp32Token` (ACTIVE) |
| Signing worker | `PlainSigner` (ACTIVE) |
| Keystore | `/opt/signserver/res/test/dss10/esp32signer.p12` |
| Key alias | `esp32signer` (RSA 3072) |
| Keystore password | `foo123` |
| Signature algorithm | `NONEwithRSAandMGF1` |
| Client-side hashing | `true`, digest `SHA-256` |

### Deterministic manual setup (Admin Web, no REST)

1. Build a p12 keystore with the RSA-3072 key and its certificate:

   ```bash
   # from /workspace
   openssl req -new -x509 -key keys/signing_key.pem \
     -out tmp/signserver-keys/esp32signer.pem -days 3650 -subj "/CN=esp32signer"
   openssl pkcs12 -export \
     -inkey keys/signing_key.pem \
     -in tmp/signserver-keys/esp32signer.pem \
     -out tmp/signserver-keys/esp32signer.p12 \
     -name esp32signer -passout pass:foo123
   ```

2. Copy it into the container and fix permissions:

   ```bash
   docker cp tmp/signserver-keys/esp32signer.p12 \
     signserver:/opt/signserver/res/test/dss10/esp32signer.p12
   docker exec -u root signserver \
     chmod 644 /opt/signserver/res/test/dss10/esp32signer.p12
   ```

3. In `https://localhost:8444/signserver/adminweb/`, delete any old workers.

4. Create `Esp32Token`:
   **Add Worker → From Template → `keystore-crypto.properties`**

   | Field | Value |
   |---|---|
   | `NAME` | `Esp32Token` |
   | `KEYSTOREPATH` | `/opt/signserver/res/test/dss10/esp32signer.p12` |
   | `KEYSTORETYPE` | `PKCS12` |
   | `KEYSTOREPASSWORD` | `foo123` |

   Apply → **Crypto Token** → **Activate** → `foo123`. Storing
   `KEYSTOREPASSWORD` makes the token **auto-activate** after a container
   restart, so no manual activation is needed.

5. Create `PlainSigner`:
   **Add Worker → From Template → `plainsigner.properties`**

   | Field | Value |
   |---|---|
   | `NAME` | `PlainSigner` |
   | `CRYPTOTOKEN` | `Esp32Token` |
   | `DEFAULTKEY` | `esp32signer` |
   | `SIGNATUREALGORITHM` | `NONEwithRSAandMGF1` |
   | `CLIENTSIDEHASHING` | `true` |
   | `ACCEPTED_HASH_DIGEST_ALGORITHMS` | `SHA-256` (template sets `SHA-256,SHA-384,SHA-512`) |
   | `AUTHTYPE` | `NOAUTH` |
   | `TYPE` | `PROCESSABLE` |

   Apply. If `Esp32Token` was deactivated by the reload, re-activate it with
   `foo123`. Both workers must be **ACTIVE**.

### Signing (verified)

```bash
cd blinky
openssl dgst -sha256 -binary build/blinky-unsigned.bin | base64 -w0 \
  > build/blinky.sha256.b64

curl --fail-with-body --silent --show-error -k \
  --cert ../tmp/signserver-keys/client.crt \
  --key ../tmp/signserver-keys/client.key \
  -H 'X-Keyfactor-Requested-With: REST' \
  -H 'Content-Type: application/json' \
  -H 'Accept: application/json' \
  --data "{\"data\":\"$(cat build/blinky.sha256.b64)\",\"encoding\":\"BASE64\",\"metaData\":{\"USING_CLIENTSUPPLIED_HASH\":\"true\",\"CLIENTSIDE_HASHDIGESTALGORITHM\":\"SHA-256\"}}" \
  'https://localhost:8444/signserver/rest/v1/workers/PlainSigner/process' \
  > build/signserver-response.json

jq -r .signerCertificate build/signserver-response.json \
  | base64 -d > build/signer-certificate.der
openssl x509 -inform DER -in build/signer-certificate.der -pubkey -noout \
  > ../tmp/signserver-keys/signing_public.pem
jq -r .data build/signserver-response.json \
  | base64 -d > build/signature.bin

espsecure sign-data --version 2 \
  --pub-key ../tmp/signserver-keys/signing_public.pem \
  --signature build/signature.bin \
  --output build/blinky.bin \
  build/blinky-unsigned.bin

espsecure verify-signature --version 2 \
  --keyfile ../tmp/signserver-keys/signing_public.pem build/blinky.bin
```

Expected verification output:

```text
Signature block 0 is valid (RSA).
Signature block 0 verification successful using the supplied key (RSA).
```

Flash the bootloader, partition table, OTA data, and the signed application:

```bash
python -m esptool --chip esp32 -b 460800 \
  --before default-reset --after hard-reset write-flash \
  --flash-mode dio --flash-freq 40m --flash-size keep \
  0x1000 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0xe000 build/ota_data_initial.bin \
  0x10000 build/blinky.bin
```

### Findings / gotchas

- **ESP-IDF 6.0 requires RSA-3072** for Secure Boot v2 remote signing
  (`espsecure` rejects any other RSA size: *"Secure boot v2 only supports
  RSA-3072"*). The token key must be RSA-3072.
- **`KeystoreCryptoToken` cannot self-sign CSRs.** “Generate CSR → CSR signed
  by worker” returns *Not supported* on 7.3.2. Bundle the key and a certificate
  into a p12 and load the p12 as the token keystore instead.
- **`NOCERTIFICATES=true` is ignored on 7.3.2** (supported since 7.4.0).
  `PlainSigner` genuinely needs a signer certificate. The p12 must contain the
  certificate together with the key.
- **Reloading workers deactivates the crypto token.** After any config reload,
  re-activate the token in the Admin Web with the keystore password. Setting
  `KEYSTOREPASSWORD` on the crypto worker auto-activates it after a container
  restart; without it, every restart requires manual activation.
- **`docker cp` sets root-owned `600` permissions**, which SignServer cannot
  read. Run `docker exec -u root signserver chmod 644 ...` after copying.
- **The digest algorithm name must match the worker.** The template's
  `ACCEPTED_HASH_DIGEST_ALGORITHMS` uses `SHA-256` (hyphenated); use
  `CLIENTSIDE_HASHDIGESTALGORITHM=SHA-256` in the request, not `SHA256`.
- **`-k` was used for local TLS verification** because `tmp/signserver-keys/ca.crt`
  (Local Test CA) signs the client cert but not the SignServer server cert.
  Provide the correct server CA for `--cacert` in production.
- **Public key for `espsecure` comes from the signer certificate** returned by
  SignServer (or the key's CSR), never from a private key on the build machine.
