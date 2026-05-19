### 四个核心子命令

| 命令 | 用法 | 说明 |
| ---- | ---- | ---- |
| `list` | `settings list` | 调用 `kvdb_enumerate_keys` 遍历所有键，并对每个键再调用 `kvdb_get` 取出值，以 `key=value` 形式输出（控制字符被转义，便于二进制安全显示） |
| `get` | `settings get <key>` | 调用 `kvdb_get`，找不到时打印 `key not found` 并返回退出码 2；I/O 错误返回 1 |
| `put` | `settings put <key> <value>` | 调用 `kvdb_set` 写入；I/O 或其他错误返回 1 |
| `delete` | `settings delete <key>` | 调用 `kvdb_delete` 删除键，找不到时打印 `key not found` 并返回退出码 2；I/O 错误返回 1 |

### 数据库文件来源（按优先级）
1. 命令行 `-f <db-file>`
2. 环境变量 `SETTINGS_DB`
3. 默认值 `settings.kvdb`

### 关键实现细节
- 默认采用 `kvdb` 的 LZ4 压缩存储（`kvdb_new` 的默认值），无需额外干预。
- `list` 把对每个 key 的值读取写在回调里，回调一旦遇到 I/O 错误就把 `*stop = 1` 并把错误码透传给 `main`，避免静默成功。
- 输出会转义 `\n / \r / \t / \\ / 不可见字节`，确保 `list` 不会被二进制内容破坏终端显示。
- 退出码：`0` 成功；`1` I/O / 内部错误；`2` 键不存在；`64` 使用方式错误（与 `sysexits.h` 中的 `EX_USAGE` 一致）。

### 构建

```bash
./build.sh
```

## 用例演示

```bash
# 写入
./settings put name "Alice"
./settings put theme dark

# 读取单个键
./settings get name
# -> Alice

# 列出全部
./settings list
# -> name=Alice
#    theme=dark

# 删除某个键
./settings delete theme
./settings list
# -> name=Alice

# 通过环境变量指定数据库
SETTINGS_DB=/tmp/app.kvdb ./settings list
```