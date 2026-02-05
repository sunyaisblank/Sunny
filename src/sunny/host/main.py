"""Sunny MCP Server Entry Point.

Component: HSMN001A
Domain: HS (Host) | Category: MN (Main)

Primary entry point for running the Sunny MCP server.
This module bootstraps the application and starts the server.
"""

from __future__ import annotations

import asyncio
import logging
import sys


def configure_logging() -> None:
    """Configure application logging."""
    log_level = logging.INFO

    # Check for debug mode
    if "--debug" in sys.argv or "-d" in sys.argv:
        log_level = logging.DEBUG

    logging.basicConfig(
        level=log_level,
        format="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
        handlers=[logging.StreamHandler()],
    )


def main() -> None:
    """Run the MCP server."""
    configure_logging()
    logger = logging.getLogger("sunny.host")

    try:
        # Import server components
        # This triggers tool registration via the tools/__init__.py imports
        from sunny.application.server.mcp import mcp
        from sunny.application.server import tools  # noqa: F401 - triggers registration

        logger.info("Starting Sunny MCP Server...")
        asyncio.run(mcp.run())

    except KeyboardInterrupt:
        logger.info("Server shutdown requested")
    except Exception as e:
        logger.error(f"Server error: {e}")
        raise


if __name__ == "__main__":
    main()
