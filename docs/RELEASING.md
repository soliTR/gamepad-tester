# Making a release

1. Set the new version in `project(... VERSION x.y.z)` in `CMakeLists.txt` and add a section `## [x.y.z] - date` to `CHANGELOG.md`.
2. Commit, then tag and push: `git tag vx.y.z && git push origin main vx.y.z`.
3. The release workflow builds, tests, attests and publishes the release. It refuses to run if the tag and the version in `CMakeLists.txt` differ.
