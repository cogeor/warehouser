"""Result type for error handling without exceptions."""

from typing import Generic, TypeVar

T = TypeVar("T")


class Result(Generic[T]):
    """Simple Result type for fallible operations.

    Use Result.ok(value) for success, Result.err(message) for failure.
    This avoids using exceptions for control flow.
    """

    def __init__(self, value: T | None = None, error: str | None = None) -> None:
        self._value = value
        self._error = error

    @classmethod
    def ok(cls, value: T) -> "Result[T]":
        """Create a successful result with a value."""
        return cls(value=value)

    @classmethod
    def err(cls, error: str) -> "Result[T]":
        """Create a failed result with an error message."""
        return cls(error=error)

    def is_ok(self) -> bool:
        """Check if the result is successful."""
        return self._error is None

    def is_err(self) -> bool:
        """Check if the result is an error."""
        return self._error is not None

    def unwrap(self) -> T:
        """Get the value, raising ValueError if error."""
        if self._error is not None:
            raise ValueError(self._error)
        return self._value  # type: ignore

    def unwrap_or(self, default: T) -> T:
        """Get the value or return default if error."""
        if self.is_ok():
            return self._value  # type: ignore
        return default

    def error(self) -> str | None:
        """Get the error message, if any."""
        return self._error

    def map(self, func: "Callable[[T], U]") -> "Result[U]":
        """Apply a function to the value if successful."""
        if self.is_ok():
            return Result.ok(func(self._value))  # type: ignore
        return Result.err(self._error)  # type: ignore


from typing import Callable, TypeVar

U = TypeVar("U")
