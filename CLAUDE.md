# QtNotes

## Conventions

Prefer Qt's own types and helpers over the C++ standard library where Qt has a current equivalent:

- Smart pointers: `QScopedPointer`, `QSharedPointer`, `QPointer`, etc., not `std::unique_ptr` or `std::shared_ptr`.
- Min/max/clamp: `qMin`, `qMax` and `qBound(min, value, max)`, not `std::min`, `std::max` or `std::clamp`. Note that `qBound` takes the bounds first, and `qMin`/`qMax` need both arguments to be the same type.

Use the standard library where Qt deprecated or removed its own version in favour of it:

- `std::as_const`, not `qAsConst` (deprecated in Qt 6.6).
- `std::sort`, `std::find_if` and other `<algorithm>` functions (Qt 6 removed `qSort`, `qFind`, etc.).
- `std::chrono` durations for times, e.g. `QTimer::setInterval(750ms)`.

## Scope

Notes are expected to contain only Markdown this app wrote: the editor's content, saved by `TextFormatter::markdown()`. Import accepts any `.md` files, but notes from other sources aren't supported. So:

- Saving and reloading anything the editor can produce must be lossless. That includes literal text that looks like Markdown, such as `~`, `_` or `*`.
- Don't add handling for Markdown the editor can't produce, such as horizontal rules, block quotes, tables, code spans or HTML. Mention it in a sentence if it's relevant, but don't fix it.
- Security and privacy problems with imported notes aren't a priority while notes only come from the user. Revisit that if notes ever come from other people, e.g. through sharing.
