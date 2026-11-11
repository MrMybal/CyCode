---
id: update-storage
title: Downloads, storage and update history
description: Resume a download, review retained payloads and interpret historical receipts.
section: Work in CyCode
parent: updates
order: 291
revision: 1
---

## Resume a download {#downloads}

Application updates separates checking, downloading and installation. An
interrupted compatible download can offer Resume or a fresh download. The
recorded asset and accepted HTTP range must match; the complete SHA-256 is still
verified before installation is offered. A retained partial file is not an
installable update. Progress/rate estimates describe the current phase.

## Review update storage {#storage}

Choose **Inspect update storage** explicitly. Select an eligible older download
and **Review retirement**; inspect version, directory, payloads and sizes, then
acknowledge the action before Retire reviewed download. Retirement moves the
owned payloads into retained storage: it does not free disk space. Current/newer
versions, recovery backups, unknown files and unsupported legacy caches stay protected.

Inspect again and review cache restoration to move retained payloads back.
Occupied destinations or modified files block restoration. Keep the receipt after
a cancelled/partial result and inspect it again. There is no permanent-delete
or automatic-expiry action. Eligibility applies to supported managed Windows x64
portable installations; development checkouts are refused.

## Read historical outcomes {#history}

**Inspect detailed update history** reads retained metadata without installing,
restoring or rechecking payloads. Filter text/category and use the paged list.
Select a row for prepared/completed counts, installation/recovery stages and
reported outcome. Older undated records say Date unavailable; a recorded date
does not establish trusted chronology.

History describes an earlier attempt, not the current installation's hashes or
restoration eligibility. Waiting, failed, restart-failed and cancelled states
need their own review. A restart failure can follow a successful restoration.
For a blocked installation, use [interrupted-update recovery](doc:update-recovery#recover).
