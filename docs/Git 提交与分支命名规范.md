# Git 提交与分支命名规范



## 1.分支命名规范 (Branch Naming)

分支名称应清晰地反映其意图和范围。所有分支名均使用**小写字母**，单词之间用**连字符 `-`** 分隔。

### 2.1 分支结构

采用 `类型/简短描述` 的结构。在大型项目中，可加入可选的 `范围`。

```
<type>/<description>
// 或
<type>/<scope>/<description>
```

### 2.2 分支类型 (`<type>`)

| **类型**   | **描述**                                 |
| ---------- | ---------------------------------------- |
| `feature`  | 用于开发新功能。                         |
| `fix`      | 用于修复已发现的缺陷（Bug）。            |
| `hotfix`   | 用于修复线上环境的紧急 Bug。             |
| `docs`     | 用于添加或修改文档。                     |
| `refactor` | 用于代码重构，不增加新功能也不修复 Bug。 |
| `style`    | 用于代码格式化，不影响代码逻辑。         |
| `test`     | 用于添加或修改测试用例。                 |
| `chore`    | 用于日常杂务，如构建流程、依赖库的变更。 |

### 2.3 命名示例

| **场景**               | **推荐命名**                    | **不推荐命名**                     |
| ---------------------- | ------------------------------- | ---------------------------------- |
| 开发用户登录功能       | `feature/user-authentication`   | `feature/UserAuth` (大小写混用)    |
| 修复支付模块的签名错误 | `fix/payment/signature-error`   | `fix/fix_payment_bug` (使用下划线) |
| 添加 README 使用说明   | `docs/update-readme`            | `docs/readme` (过于简单)           |
| 重构电源管理模块       | `refactor/power-manager-module` | `refactor/powermgr` (使用缩写)     |
| 修复紧急的内存泄漏问题 | `hotfix/memory-leak-in-v1.2.1`  | `hotfix/test` (过于通用)           |

## 2. 提交信息规范 (Commit Message)

我们遵循 **Conventional Commits** 规范，它与分支命名规范相辅相成，便于生成修改日志 (Changelog)。

### 3.1 提交信息结构

```
<type>(<scope>): <subject>
<-- 空一行 -->
[optional body]
<-- 空一行 -->
[optional footer]
```

### 3.2 头部 (`<type>(<scope>): <subject>`)

- **`type`**: 与分支类型一致，如 `feat`, `fix`, `docs`, `refactor` 等。
- **`scope` (可选)**: 括号内填写本次提交影响的范围，如模块名 `(uart)`, `(power-manager)`。
- **`subject`**:
  - 用一句话清晰描述本次提交的目的（不超过50个字符）。
  - 使用动词开头的祈使句，如 `add`, `update`, `fix`，而非 `added`, `updates`。
  - 结尾不加英文句号 `.`。

### 3.3 主体 (`body` - 可选)

对 `subject` 进行更详细的阐述，说明修改的**动机**和**前后的行为对比**。

### 3.4 脚注 (`footer` - 可选)

- 用于标记**重大变更 (Breaking Changes)** 或**关闭任务 (Closes Issues)**。
- 示例：`BREAKING CHANGE: API endpoint /v1/users has been deprecated.`
- 示例：`Closes #123, #456`

### 3.5 提交信息完整示例

```
feat(auth): add password hashing and salting

Implement password hashing using bcrypt to enhance user security.
The previous implementation stored passwords in plaintext, which posed a significant security risk.

Closes #245
BREAKING CHANGE: User password format in the database has changed and requires migration.
```

## 3. 标签命名规范 (Tag Naming)

标签用于标记重要的版本发布点，我们遵循 **Semantic Versioning (SemVer)** 规范。

- **格式**: `v<MAJOR>.<MINOR>.<PATCH>`
- **示例**:
  - `v1.0.0`: 第一个正式版本。
  - `v1.0.1`: 修复了 Bug 的补丁版本。
  - `v1.1.0`: 增加了新功能，但向后兼容的次要版本。
  - `v2.0.0`: 包含不向后兼容的重大变更的主要版本。
  - `v2.0.0-beta.1`: 预发布版本（如测试版）。