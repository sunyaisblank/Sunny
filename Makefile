# Sunny Makefile
# Development task automation

.PHONY: install test lint format typecheck check clean run help

# Default target
all: check

# Install dependencies
install:
	uv sync --all-extras

# Run tests
test:
	uv run pytest --cov=src/sunny --cov-report=term-missing

# Run tests with coverage report
test-cov:
	uv run pytest --cov=src/sunny --cov-report=xml --cov-report=html

# Lint code
lint:
	uv run ruff check .

# Format code
format:
	uv run ruff format .
	uv run ruff check --fix .

# Type checking
typecheck:
	uv run mypy src/sunny --ignore-missing-imports

# Run all checks
check: lint typecheck test

# Clean build artifacts
clean:
	rm -rf .pytest_cache
	rm -rf .mypy_cache
	rm -rf .ruff_cache
	rm -rf htmlcov
	rm -rf .coverage
	rm -f coverage.xml
	find . -type d -name "__pycache__" -exec rm -rf {} + 2>/dev/null || true
	find . -type f -name "*.pyc" -delete 2>/dev/null || true

# Run the server
run:
	uv run python -m sunny.server

# Show help
help:
	@echo "Sunny - MCP Server for Ableton Live"
	@echo ""
	@echo "Usage:"
	@echo "  make install    Install dependencies with uv"
	@echo "  make test       Run tests with coverage"
	@echo "  make lint       Run ruff linter"
	@echo "  make format     Format code with ruff"
	@echo "  make typecheck  Run mypy type checker"
	@echo "  make check      Run all checks (lint, typecheck, test)"
	@echo "  make clean      Remove build artifacts"
	@echo "  make run        Run the MCP server"
	@echo "  make help       Show this help"
