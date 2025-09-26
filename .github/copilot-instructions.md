---
applyTo: '**'
---

# General Instructions


## C Coding Guidelines

This project follows C coding standards suitable for OS kernel development. Key points:

- **Type Safety:** Use appropriate types, avoid unsafe casts, and prefer explicit type declarations.
- **Resource Management:** Manually manage memory with care, use allocation functions properly, and avoid leaks. In kernel context, use kmalloc/kfree or similar kernel allocators.
- **Initialization:** Always initialize variables upon declaration. Use designated initializers for structs.
- **Const Correctness:** Use `const` for read-only data and pointers where applicable.
- **Error Handling:** Use return codes or errno for error signaling. Avoid exceptions as C does not support them.
- **Functions:** Keep functions short and focused. Pass by value for small types, by pointer for large types or to modify arguments.
- **Structs:** Use structs for data aggregation. Encapsulate data appropriately, avoiding global state where possible.
- **Macros and Preprocessor:** Use macros sparingly; prefer inline functions, enums, or constants for better type safety.
- **Standard Library:** Limited in kernel environments; implement custom utilities or use kernel-specific equivalents.
- **Naming:** Use descriptive names. Types/structs: `CamelCase`. Functions/variables: `snake_case`.
- **Comments & Documentation:** Write clear comments for non-obvious code. Document public APIs with Doxygen-style comments.
- **Code Style:** Follow consistent formatting (indentation, braces, spacing). Use tools like `clang-format`.

For more details, see the [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines).


## GIT Repository Management

### Commits

Use the Conventional Commits format: `<type>(scope): short summary`

Examples:

- `feature(config): support dynamic environment loading`
- `fix(core): handle missing config file gracefully`
- `test(utils): add unit tests for retry logic`

Allowed types (use these as `<type>` in your commit messages):

- `feature` – New features
- `fix` – Bug fixes
- `documentation` – Documentation changes only
- `style` – Code style, formatting, missing semi-colons, etc. (no code meaning changes)
- `refactor` – Code changes that neither fix a bug nor add a feature
- `performance` – Code changes that improve performance
- `test` – Adding or correcting tests
- `build` – Changes to build system or external dependencies
- `ci` – Changes to CI configuration files and scripts
- `chore` – Maintenance tasks (e.g., updating dependencies, minor tooling)
- `revert` – Reverting previous commits
- `security` – Security-related improvements or fixes
- `ux` – User experience or UI improvements

Other Notes:

- Prefer simple, linear Git history. Use rebase over merge where possible.
- Use `pre-commit` hooks to enforce formatting, linting, and checks before commits.
- If unsure about a change, open a draft PR with a summary and rationale.

### Release Guidelines

We follow a simplified Git-flow model for releases:

#### Branches

- `main`: Represents the latest stable, released version. Only hotfixes and release merges are committed directly to `main`.
- `develop`: Integration branch for ongoing development. All new features and bug fixes are merged into `develop`.
- `feature/<feature-name>`: Used for developing new features. Branch off `develop` and merge back into `develop` upon completion.

Here is the release process:

1. Prepare `develop` for Release:
    - Ensure all desired features and bug fixes are merged into `develop`.
    - Update `CHANGELOG.md` with changes for the new version, with the help of the command: `git log --pretty=format:"- (%h) %s" ...`
    - Update version numbers in relevant project files (e.g., VERSION file, CMakeLists.txt, or other configuration files).
2. Make sure we start from a clean state:
    - Make sure you are on the `develop`, and that we start from there.
    - Perform final testing and bug fixing on this branch.
3. Merge to `main` and Tag:
    - Once the develop branch is stable, merge it into `main`:
      1. `git checkout main`
      2. `git merge --no-ff develop`  
    - Tag the release on `main`: `git tag -a v<version-number> -m "Release v<version-number>"`
    - Ask the user to push the changes to the `main` branch, including tags: `git push origin main --tags`
4. Merge back to `develop`:
    - Merge the main branch back into `develop` to ensure `develop` has all release changes:
      1. `git checkout develop`
      2. `git merge --no-ff main`
    - Ask the user to push the changes to `develop` branch: `git push origin develop`