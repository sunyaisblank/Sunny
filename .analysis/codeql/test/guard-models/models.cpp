// Parser/type-model fixtures, never executed. Full-repository analysis separately
// extracts the real Beat class and standard library. These definitions isolate
// constructor, template, alias and reference shapes without platform headers.
namespace sunny {
namespace core {
enum class ErrorCode { InvalidBeat };
class Beat {
  public:
    Beat() = default;
    Beat(long numerator, long denominator);
    static int from_ratio(long numerator, long denominator);
};
} // namespace core
} // namespace sunny

namespace std {
template <typename T, typename E> class expected {
  public:
    expected& operator=(const expected&);
    explicit operator bool() const;
    T& operator*();
};
template <typename E> class expected<void, E> {};
} // namespace std

using sunny::core::Beat;
using BeatAlias = Beat;
using Result = std::expected<int, sunny::core::ErrorCode>;

struct MxmlBeatUnit {
    const char* name;
    bool dotted;
};

namespace other {
struct Beat {
    int numerator;
    int denominator;
};
struct expected_value {};
expected_value unrelated_result();
} // namespace other

Result fallible();
std::expected<void, sunny::core::ErrorCode> fallible_void();
Result& borrowed_result();
Result* result_pointer();

struct FallibleAssignment {
    Result operator=(int);
};

void invalid_beats() {
    const Beat brace{1, 0}; // Alert: exact Beat constructor, zero denominator.
    const Beat paren(1, 0); // Alert: parenthesized construction.
    constexpr int zero = 0;
    const BeatAlias alias{1, zero}; // Alert: alias and constant expression.
}

void valid_beats() {
    const Beat zero_numerator{0, 4};
    const Beat nonzero_denominator{1, 4};
    const MxmlBeatUnit unit{"quarter", false};
    const other::Beat unrelated{1, 0};
    const auto rejected = Beat::from_ratio(1, 0); // Factory is not a constructor.
    (void)rejected;
}

void discarded_results() {
    fallible(); // Alert: alias resolves to std::expected value.
    fallible_void(); // Alert: std::expected<void, ErrorCode>.
    FallibleAssignment assignment;
    assignment = 1; // Alert: custom fallible operator= returns expected by value.
}

void retained_results() {
    Result result = fallible();
    result = fallible(); // Assignment returns a reference; error remains in result.
    if (!result) return;
    (void)*result;
    borrowed_result(); // Borrowed state is outside discarded-by-value scope.
    result_pointer(); // Pointer is outside discarded-by-value scope.
    other::unrelated_result(); // Name similarity is insufficient.
}
