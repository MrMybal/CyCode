---
id: other-providers
title: SVN, Mercurial and Diversion
description: Choose the provider whose workflow matches your existing repository.
section: Work in CyCode
parent: providers
order: 313
revision: 1
---

## Subversion {#svn}

Install SVN, prepare a working copy and enable `cycode.svn`. Map that working-copy folder in Version control; `CYCODE_SVN` can select an absolute client. Status, diff, history and annotations use the common views. Review explicit add/revert/commit/update actions before executing them. Selecting the provider does not checkout a repository. SVN authentication and server configuration remain in the external client.

## Mercurial {#hg}

Enable `cycode.mercurial`, install `hg` and map the exact local repository containing `.hg`. Use `CYCODE_HG` for an absolute executable override. Add tracks one file; stop tracking preserves its working file. Commit includes tracked changes and does not automatically add unknown files. Revert requires `REVERT` and does not create an `.orig` backup.

Update requires a clean tracked working copy and one resolved changeset. Pull uses a configured alias without updating files; Push uses a configured alias without force or new-branch permission. The default alias is `default`. Arbitrary URLs and revset expressions are not accepted by these forms.

## Diversion {#diversion}

Install and authenticate `dv`, then clone/init with that client. Enable `cycode.diversion` and map the exact directory containing `.diversion`. An absolute `CYCODE_DV` selects the client. Diversion's own agent synchronises working changes; it has no Git staging area or separate Push/Pull controls here.

Review sync readiness before writes. Commit requires a message; the ordinary action commits all changes and the agent publishes the result. Branch switching and Update require a clean, fully synced working copy. Revert affects one file after `REVERT`. Lock availability depends on the installed service/plan. An incomplete sync state blocks mutations.
