# QtNotes

## Conventions

Prefer Qt's own types and helpers over the C++ standard library where Qt has a current equivalent:

- Smart pointers: `QScopedPointer`, `QSharedPointer`, `QPointer`, etc., not `std::unique_ptr` or `std::shared_ptr`.
- Min/max/clamp: `qMin`, `qMax` and `qBound(min, value, max)`, not `std::min`, `std::max` or `std::clamp`. Note that `qBound` takes the bounds first, and `qMin`/`qMax` need both arguments to be the same type.

Use the standard library where Qt deprecated or removed its own version in favour of it:

- `std::as_const`, not `qAsConst` (deprecated in Qt 6.6).
- `std::sort`, `std::find_if` and other `<algorithm>` functions (Qt 6 removed `qSort`, `qFind`, etc.).
- `std::chrono` durations for times, e.g. `QTimer::setInterval(750ms)`.
