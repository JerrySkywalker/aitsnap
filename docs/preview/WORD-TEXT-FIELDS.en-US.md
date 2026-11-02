# Word report text and fields

Open Settings → Word report. Choose a preset or configure Batch title, Image caption and Context separately. The preview works without starting Word.

## Text rules

- Enter fixed text, such as `Test result`, or combine it with fields: `Capture {sequence} · {profile}`.
- Only enabled sections are written. Images only disables all three visible text sections. Required Word image/table structure paragraphs still exist.
- Title and context are written once per batch; captions are expanded for each output image. Each switch is independent.
- Press Enter for multiple lines. The two literal characters `\n` are not converted into a newline.
- Field names are case-sensitive. Each template has a 4096-character limit. Unknown fields and unmatched braces report their location for enabled templates.
- Use `{{` for a literal `{` and `}}` for a literal `}`. Example: `{{fixed}} {profile}`.

## Fields available in all sections

| Field | Meaning |
| --- | --- |
| `{profile}` | Capture profile name |
| `{datetime}` | Capture UTC timestamp in ISO format, for example `2026-11-02T04:00:00.000Z`; Z means UTC, not local time |
| `{sequence}` | Capture sequence; empty for older batches without this value |
| `{test}` | Test identifier |
| `{device}` | Device identifier |
| `{station}` | Station identifier |
| `{operator}` | Operator |
| `{run}` | Linked run identifier |
| `{step}` | Linked step execution identifier |
| `{command}` | Linked command invocation identifier |
| `{event}` | Linked event identifier |

Missing information becomes an empty string. Run/step fields come from the selected context. These are named substitutions, not arbitrary Word fields or expressions; extra date-format directives are not supported.

## Caption-only fields

| Field | Original image | Composite |
| --- | --- | --- |
| `{name}` | Region name | Scene name |
| `{label}` | Number and name, e.g. `#2 Voltage` | Scene name and revision, e.g. `Comparison r3` |
| `{number}` | Region number | Empty |
| `{kind}` | `ORIGINAL` | `COMPOSITE` |
| `{scene_revision}` | Empty | Scene revision |

These image fields cannot be used in the title or context. Each image has its own values.

## Examples

Title: `Test {test} · capture {sequence}`

Caption: `{label} | {datetime}`

Context (press Enter to create actual lines):

```text
Device: {device}
Station: {station}    Operator: {operator}
Run: {run}    Step: {step}
```

To write only a fixed caption, disable title and context and set caption to `Test result`. Choose Images only to suppress all visible text.

## Saving and retry

Apply the settings. Resolved text is frozen when a report job is created; later template, region-name or language changes do not alter queued/retried jobs. Adding a historical batch uses its captured image/time/sequence snapshot with the text settings selected when it is added.

Capture and copy does not save images or enqueue a Word job. Save actions can enqueue reports when enabled; saved batches can also be added manually. Review layout in a synthetic document first. Queue state and actual document appearance are separate checks; do not blindly repeat a write whose result is uncertain.
