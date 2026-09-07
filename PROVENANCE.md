# Provenance record

This file exists so authorship of **StreamStat Engine** can be verified
later, even if the code (or a compiled binary of it) turns up somewhere
else under a different name.

## Author

- Name: Erkan
- Email: erkantahaguney@gmail.com
- Project UUID (compiled into the binary, see `include/provenance.h`):
  `4c128310-5fab-438f-8f65-4137092a8976`

## Source snapshot hash

Computed as: `sha256sum` of every tracked source/doc/config file (sorted
by path), then `sha256sum` of that combined listing — i.e. a hash of
hashes, so the record below both fixes the exact byte content of every
file and is easy to reproduce independently.

```
Snapshot timestamp (UTC): 2026-09-07T14:07:05Z
Combined hash (SHA-256):  f7180cae2652681bdeb9211dc9aeb6debf4799709907e43c4eb6367e83d63cde

Per-file SHA-256:
01f0af790a473d7c3c4e2b9c9540a6469608fd3dd7c4f5fdc458a444aff56ca8  ./.gitignore
1217bf15cc515a337e27c02175af0521ac6bc98931f6a41dcc3f638cc1dbdc6b  ./LICENSE
0c3507fe58a46a5a2592edf9b6da96752e934da79bfae0747a3c676f9787e52e  ./Makefile
7917e9652c4ed32018350de902cfefddc354b55e7d4394f859ef66e805e59a27  ./README.md
339bf3dc057b12f89673378524a0f8886112d00b66ecce072540ea0eeb358e2a  ./docs/gorev.md
654a3c58d6332deb1c32510be5d2e5fc2f3e5c088f3c026acb2af64e10b79c4f  ./include/arena.h
b8e75e904400e2b6e6fd5cf79c4ca1d8c4c6ac744c837dd29474085322baa3c5  ./include/csv_parser.h
ff33677fcd632342e43f382596c3a618521e80d2d6c950b57fc4be4c4a20f290  ./include/provenance.h
29a1a79ba44c8a4ee75ba81d43c86ae12f38cbdae3d5af9240bde2183aa57b71  ./include/stats.h
7a62f8ac6b62ad4739b0168539bab8b231aae0b2698879b44b1411259bacf323  ./src/arena.c
11cb450aa662bddf71e9dbe920efa88fbcf7cbea653aedbce4cd786a20cb0123  ./src/csv_parser.c
a7b099e76c3c52357825691bcde10c330a112a234c91b767da308974ae76969a  ./src/main.c
481c5be864cf497b6425513b49332df70e2d1640aabb50f72d4a488a29739350  ./src/stats.c
b75951b479f46b91ca8664c9390053dedab94218e26cf58821ba24bfb6dc508f  ./tests/sample.csv
c0039e0f023d6f1d1743b6a53a75fa39c43f1f00721408f1fdb7a972ae706bd6  ./tools/gen_test_csv.c
```

## How to reproduce this hash

From the project root:

```
find . -type f \( -name "*.c" -o -name "*.h" -o -name "Makefile" \
  -o -name "*.md" -o -name "LICENSE" -o -name ".gitignore" -o -name "*.csv" \) \
  ! -path "*/build/*" ! -name "PROVENANCE.md" | sort | xargs sha256sum > filehashes.txt
sha256sum filehashes.txt
```
(Compare the per-file list too — the combined hash alone only tells you
*something* changed, not *what*.)

## Git commit record

The initial commit of this exact snapshot was made locally with author
`Erkan <erkantahaguney@gmail.com>`; its commit hash is recorded in the git
history itself (`git log`) and, if/when this repository is pushed to a
hosting service (e.g. GitHub), that service's own commit timestamp
becomes an additional, independently-dated proof of authorship.

## What this does and doesn't prove

This is a lightweight, honest provenance trail — a timestamped hash plus
an embedded binary fingerprint (`./streamstat --about`) — not a
cryptographic non-repudiation scheme. It is meant to make it easy to
demonstrate "this exact code existed, with this UUID and this author
listed, as of this date," which is normally enough to resolve an
authorship dispute over a take-home assignment. It cannot, by itself,
stop someone from copying the code; combine it with keeping this
repository private and/or emailing yourself (or a trusted third party)
this file's hash for an independent timestamp if stronger proof is ever
needed.
