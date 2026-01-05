"""Tests for Result type."""

import pytest

from training.utils.result import Result


class TestResult:
    def test_ok_is_ok(self) -> None:
        result = Result.ok(42)
        assert result.is_ok() is True
        assert result.is_err() is False

    def test_err_is_err(self) -> None:
        result: Result[int] = Result.err("something went wrong")
        assert result.is_ok() is False
        assert result.is_err() is True

    def test_unwrap_ok(self) -> None:
        result = Result.ok("hello")
        assert result.unwrap() == "hello"

    def test_unwrap_err_raises(self) -> None:
        result: Result[str] = Result.err("error message")
        with pytest.raises(ValueError, match="error message"):
            result.unwrap()

    def test_unwrap_or_returns_value(self) -> None:
        result = Result.ok(42)
        assert result.unwrap_or(0) == 42

    def test_unwrap_or_returns_default(self) -> None:
        result: Result[int] = Result.err("error")
        assert result.unwrap_or(0) == 0

    def test_error_returns_message(self) -> None:
        result: Result[int] = Result.err("something failed")
        assert result.error() == "something failed"

    def test_error_returns_none_for_ok(self) -> None:
        result = Result.ok(42)
        assert result.error() is None

    def test_map_on_ok(self) -> None:
        result = Result.ok(5)
        mapped = result.map(lambda x: x * 2)
        assert mapped.unwrap() == 10

    def test_map_on_err(self) -> None:
        result: Result[int] = Result.err("error")
        mapped = result.map(lambda x: x * 2)
        assert mapped.is_err()
        assert mapped.error() == "error"

    def test_with_complex_type(self) -> None:
        data = {"key": "value", "num": 42}
        result = Result.ok(data)
        assert result.unwrap()["key"] == "value"

    def test_with_none_value(self) -> None:
        result: Result[None] = Result.ok(None)
        assert result.is_ok()
        assert result.unwrap() is None
