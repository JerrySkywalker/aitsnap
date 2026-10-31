# Dependency DAG

```mermaid
flowchart TD
 R00 --> R01
 R00 --> R02
 R00 --> R03
 R01 --> R04
 R02 --> R04
 R03 --> R04
 R00 --> D00
 R01 --> D01
 D00 --> D01
 D01 --> D02
 D02 --> D03
 R04 --> D03
 D03 --> D04
 R02 --> D04
 D00 --> D05
 R03 --> D05
 D01 --> D06
 D05 --> D06
 D04 --> D07
 D06 --> D07
 L00 --> L01
 D01 --> L01
 D02 --> L01
 L01 --> L02
 D07 --> L02
```

Each edge means required source contract or evidence from predecessor, not an implied release approval. If a dependency has missing real hardware/Office, permit independent pure-core work and record the relevant HOLD. No cycles by design.
