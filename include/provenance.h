#ifndef PROVENANCE_H
#define PROVENANCE_H

/*
 * provenance.h — authorship fingerprint compiled into every binary.
 *
 * Why this exists: this project was written as a take-home technical
 * assessment. If the binary or source is later copied and submitted or
 * published by someone else, the strings below survive in the compiled
 * executable (`strings ./streamstat --about` or even a plain `strings
 * ./streamstat` will surface them) and act as a durable, hard-to-remove
 * claim of original authorship — removing them requires deliberately
 * editing and recompiling the source, which is itself evidence.
 *
 * This is a lightweight, honest measure (a visible watermark + a
 * timestamped, hashed source snapshot recorded in PROVENANCE.md), not a
 * DRM/obfuscation scheme — the goal is *provable authorship*, not
 * preventing anyone from reading the code.
 */

#define PROVENANCE_AUTHOR_NAME   "Erkan"
#define PROVENANCE_AUTHOR_EMAIL  "erkantahaguney@gmail.com"

/* A per-project random UUID, generated once and committed alongside the
 * initial source snapshot. It has no functional purpose — it is a nonce
 * that ties this exact codebase to the PROVENANCE.md record (and to any
 * future dated proof, e.g. an email or a private gist) with negligible
 * collision probability, so an identical UUID appearing in a copy is
 * strong circumstantial evidence of common origin. */
#define PROVENANCE_PROJECT_UUID  "4c128310-5fab-438f-8f65-4137092a8976"

/* Bump this if the fingerprint scheme itself changes. */
#define PROVENANCE_SCHEME_VERSION 1

#endif /* PROVENANCE_H */
