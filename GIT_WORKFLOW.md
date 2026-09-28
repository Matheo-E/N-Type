# Git Workflow

This document explains how Git and GitHub are used on the Dashboard project.

The project uses three levels of branches:

```text
main
  │
  └── dev
       │
       ├── backend/*
       └── frontend/*
```

The main rule is:

**1 issue = 1 feature = 1 branch = 1 Pull Request**

Feature branches are merged into `dev`.

Only validated development changes are merged from `dev` into `main`.

---

# 1. Branches

## `main`

`main` contains the stable version of the project.

Direct development on `main` is not allowed.

Changes reach `main` through a Pull Request from `dev`.

```text
dev → main
```

## `dev`

`dev` is the main development and integration branch.

Backend and frontend features are merged into `dev`.

```text
backend/* → dev
frontend/* → dev
```

The purpose of `dev` is to combine and test the different features before they are integrated into the stable `main` branch.

## Feature branches

Feature branches contain one specific task or feature.

Backend branches use:

```text
backend/<feature>
```

Frontend branches use:

```text
frontend/<feature>
```

Examples:

```text
backend/fastapi-init
backend/postgres
backend/auth
backend/weather-service

frontend/dashboard
frontend/widget-system
frontend/auth
frontend/weather-widget
```

Other branch types can be used when appropriate:

```text
docs/<topic>
fix/<topic>
chore/<topic>
refactor/<topic>
test/<topic>
```

---

# 2. Before Starting a Task

Always update `dev` before creating a new feature branch.

```bash
git switch dev
git pull origin dev
```

---

# 3. Create a Feature Branch

Create the branch from `dev`.

For a backend feature:

```bash
git switch -c backend/my-feature
```

For a frontend feature:

```bash
git switch -c frontend/my-feature
```

Examples:

```bash
git switch -c backend/postgres
```

or:

```bash
git switch -c frontend/dashboard
```

---

# 4. Work on the Feature

Only make changes related to the current Issue.

For example, if the branch is:

```text
backend/postgres
```

do not modify unrelated frontend features or authentication code.

Keep each branch focused on one task.

---

# 5. Check Your Changes

Before committing:

```bash
git status
```

Inspect the changes:

```bash
git diff
```

Run the relevant tests and checks before creating the Pull Request.

---

# 6. Commit

Add the changes:

```bash
git add .
```

Create a commit:

```bash
git commit -m "feat: add PostgreSQL database"
```

The project uses Conventional Commits.

```text
feat:      new feature
fix:       bug fix
docs:      documentation
chore:     project/configuration task
refactor:  code restructuring
test:      tests
```

Examples:

```text
feat: add FastAPI backend
feat: add PostgreSQL database
feat: add weather service
fix: fix widget refresh
docs: update Git workflow
chore: configure Docker
test: add dashboard API tests
```

---

# 7. Update the Feature Branch

Before opening the Pull Request, update the branch with the latest `dev`.

First:

```bash
git fetch origin
```

Then:

```bash
git rebase origin/dev
```

If there are no conflicts, continue normally.

---

# 8. Resolve Conflicts

If Git reports conflicts, open the affected files and resolve the conflicts.

Git may show:

```text
<<<<<<< HEAD
=======
>>>>>>> commit
```

Choose the correct code and remove the conflict markers.

Then:

```bash
git add .
```

Continue the rebase:

```bash
git rebase --continue
```

If you want to cancel the rebase:

```bash
git rebase --abort
```

---

# 9. Push the Feature Branch

For the first push:

```bash
git push -u origin backend/my-feature
```

or:

```bash
git push -u origin frontend/my-feature
```

If the branch was already pushed and the rebase changed its history:

```bash
git push --force-with-lease
```

Use `--force-with-lease` instead of `--force`.

---

# 10. Create the Pull Request

Create a Pull Request on GitHub.

Feature branches must target `dev`.

For example:

```text
backend/postgres → dev
```

or:

```text
frontend/dashboard → dev
```

The Pull Request should reference the corresponding GitHub Issue.

Example:

```text
Issue:
Add PostgreSQL database

Branch:
backend/postgres

Pull Request:
backend/postgres → dev
```

The Pull Request should contain:

* A clear title
* A short description
* The related Issue
* Tests/checks performed
* Any relevant implementation notes

When appropriate, use:

```text
Closes #123
```

This links the Pull Request to the Issue.

---

# 11. Merge into `dev`

After review and successful checks, the Pull Request can be merged into `dev`.

```text
backend/postgres
        ↓
       dev
```

or:

```text
frontend/dashboard
        ↓
       dev
```

The feature branch can then be deleted.

---

# 12. Validate `dev`

Before merging `dev` into `main`, verify that the integrated project works correctly.

Check:

* Backend
* Frontend
* Database
* Docker Compose
* Tests
* Build
* Main project features

The goal is to keep `main` stable.

---

# 13. Merge `dev` into `main`

When `dev` has been validated, create a Pull Request:

```text
dev → main
```

After review and validation, merge the Pull Request.

The resulting workflow is:

```text
backend/feature ──┐
                  │
frontend/feature ─┼──→ dev ──→ main
                  │
other/feature ────┘
```

---

# 14. After a Pull Request Is Merged

Update your local branches:

```bash
git switch dev
git pull origin dev
```

If the feature branch is no longer needed:

```bash
git branch -d backend/my-feature
```

The remote branch can also be deleted from GitHub.

---

# Daily Workflow

For a new backend feature:

```bash
git switch dev
git pull origin dev

git switch -c backend/my-feature
```

Work on the feature.

Then:

```bash
git status
git add .
git commit -m "feat: add my feature"

git fetch origin
git rebase origin/dev

git push -u origin backend/my-feature
```

Create:

```text
backend/my-feature → dev
```

After review and successful checks:

```text
backend/my-feature → dev
```

Once `dev` is validated:

```text
dev → main
```

---

# Summary

The project workflow is:

```text
GitHub Issue
      ↓
Feature Branch from dev
      ↓
Development
      ↓
Commit
      ↓
Rebase on dev
      ↓
Push
      ↓
Pull Request → dev
      ↓
Review + Tests
      ↓
Merge → dev
      ↓
Integration / Validation
      ↓
Pull Request → main
      ↓
Merge → main
```

The main rules are:

**1 Issue = 1 Feature = 1 Branch = 1 Pull Request**

**Feature branches are created from `dev`.**

**Feature branches are merged into `dev`.**

**Only validated changes from `dev` are merged into `main`.**

**Do not develop directly on `main`.**