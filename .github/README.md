# Centauri Firmware Release Process

This document outlines the automated, tag-based release process for the Centauri Carbon Motion Detector firmware. It serves as the operator's manual for creating, testing, and maintaining firmware releases.

## 1. Release Strategy Overview

Our release process is designed to be simple, robust, and automated.

- **Source of Truth**: Git tags are the single source of truth for versioning. The version number (e.g., `v1.2.3-alpha.1`) is derived directly from the tag.
- **Release Trigger**: The release workflow is automatically triggered when a tag matching the `v*` pattern is pushed to the repository.
- **Branching Model**:
  - `dev` / `feature/*`: All new development and bug fixes are merged here.
  - `build`: This is the **alpha release branch**. To prepare a release, changes from `dev` are merged into `build`.
  - `main`: This branch is reserved for future stable, production-ready releases. Releases are **not** made from `main` at this time.
- **Automation**: The `.github/workflows/release.yml` workflow handles the entire process:
  - Verifies the tag is on the correct branch (`build`).
  - Builds all firmware targets in parallel for speed.
  - Injects the version number into the firmware.
  - Gathers all binary artifacts and creates checksums.
  - Generates a changelog from commit messages.
  - Publishes a new pre-release on GitHub.

---

## 2. How to Cut a New Alpha Release (Operator's Manual)

Follow these steps to create and publish a new alpha release.

### Step 1: Prepare the `build` Branch

Merge all features and fixes intended for the alpha release from `dev` into the `build` branch.

```bash
# Switch to the build branch and make sure it's up to date
git checkout build
git pull origin build

# Merge the development branch into build
git merge dev

# Push the updated build branch
git push origin build
```

### Step 2: Create and Push the Alpha Version Tag

Create a tag on the latest commit of the `build` branch. For alpha releases, use a `-alpha` suffix.

```bash
# Follow Semantic Versioning (e.g., vMAJOR.MINOR.PATCH-alpha.N)

# Example for a first alpha release for v1.3.0
git tag v1.3.0-alpha.1

# Example for a subsequent alpha
git tag v1.3.0-alpha.2
```

Push the tag to GitHub. This will trigger the release workflow.

```bash
git push origin v1.3.0-alpha.1
```

### Step 3: Monitor the Release Workflow

1.  Go to the **Actions** tab in the GitHub repository.
2.  A new "Release Firmware" workflow will be running.
3.  Observe the jobs: The `Verify Tag on Build Branch` job runs first, followed by the parallel build jobs, and finally the `Create GitHub Release` job.

Once complete, a new pre-release will be available in the **Releases** section of the repository.

---

## 3. How to Test and Validate the Workflow

Perform this end-to-end test to confirm the system is working correctly without creating an official release.

1.  **Prepare a Test Commit**: On the `build` branch, make a trivial change (e.g., update a log file) and push it.
    ```bash
    git checkout build
    git pull
    echo "Workflow test at $(date)" >> release_test.log
    git add release_test.log
    git commit -m "chore: Test the automated release workflow"
    git push origin build
    ```

2.  **Create a Test Tag**: Use a descriptive name like `v0.1.0-alpha.test.1`.
    ```bash
    git tag v0.1.0-alpha.test.1
    git push origin v0.1.0-alpha.test.1
    ```

3.  **Monitor and Validate**:
    - Watch the workflow run in the **Actions** tab.
    - Once complete, go to **Releases**. A new pre-release should exist.
    - **Verify**: Check that the release notes, artifacts, and checksums are present.
    - **Validate Version**: Download a `firmware.bin` file and check for the version string:
      ```bash
      strings esp32-firmware.bin | grep "v0.1.0-alpha.test.1"
      ```

4.  **Clean Up**: Delete the test release from the GitHub UI and then remove the tags.
    ```bash
    # Delete the remote and local tags
    git push --delete origin v0.1.0-alpha.test.1
    git tag -d v0.1.0-alpha.test.1
    ```

---

## 4. Maintenance Guide

### Adding a New Board to the Release

When you add a new board environment to `platformio.ini` (e.g., `esp32s2`), you must also add it to the build matrix in the release workflow.

1.  **Edit the workflow file**: `.github/workflows/release.yml`
2.  **Add the new environment name** to the `matrix.env` list:

    ```yaml
    # ...
    strategy:
      matrix:
        # All build environments from platformio.ini
        env: [esp32, esp32s3, seeed_esp32s3, seeed_esp32c3, esp32c3, esp32c3supermini, esp32s2] # <-- Add new board here
    # ...
    ```

Commit this change, and the new board will be automatically built and included in the next release.