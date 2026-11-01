# RawStore

公共定义见 `include/sysal/model/raw_store.hpp`。RawStore 保存可回放的采集证据，
与归一化后的 SystemInfo 分开。

- `records`：来源、路径/命令、payload、时间和 CollectStatus；可附 ReadFailure 或 SkipReason。
- `enumerations`：具体 InventoryKind 的枚举来源、范围、条目和完成状态，独立于可选属性读取。
- `collection_request`：已知的原始 flags 与 CollectOptions；旧证据缺失时为未知。

RawSource、ReadFailure 和已有领域枚举追加新值时保持既有编号稳定。完整枚举以头文件为准，
不在文档重复维护易过期的取值表。`DfTh` 仅用于旧挂载证据的兼容解析。

## 生命周期

`System::collect(flags, options)` 验证请求后采集。只有显式请求 Collect::Raw 时，
返回对象保留 RawStore；原始请求、清单状态和无 payload 的来源观察仍可在 meta 中查看。
序列化还需指定 `SerializationOptions.include_raw` 才会输出原始 payload。
SystemCard 原生绑定在明确请求 RAW 时同时启用原始采集与输出。

`save_raw_store` / `load_raw_store` 保存和恢复 records、enumerations、collection_request，
缺少新增字段的旧证据仍可加载。`collect_from_raw` 只解析给定证据，不执行 Reader，
不按当前默认策略过滤已取得的值；回放选择的解析域与原始采集请求分别保留。
`from_json` 同样不读取当前主机。需要新采集时由调用方明确调用 collect 或 refresh。

## 状态

成功生成领域模型不证明该领域的每份清单完整。管线对照枚举身份和归一化结果生成
`meta.inventories`；空且完整才可确认范围内为零，缺少证明保持未知或已知部分。
枚举范围只描述当前接口和命名空间，不承诺整台物理机器的全部设备。
原始失败与策略跳过分别表达，说明文字仅作补充，不作为状态识别规则。
