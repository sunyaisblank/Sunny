# Sunny Documentation

This directory contains the documentation for the Sunny MCP server.

## Reading Order

The documents are arranged in order of increasing specificity. Begin with philosophy for the worldview; proceed through foundations for technical grounding; consult the guide for practical operation; reference the standards for conventions.

| Document | Purpose |
|----------|---------|
| [philosophy.md](philosophy.md) | Design worldview and assumptions |
| [foundations.md](foundations.md) | Technical foundations from first principles |
| [guide.md](guide.md) | Practical operation and usage |
| [standard.md](standard.md) | Technical standards and conventions |
| [architecture.md](architecture.md) | Component structure and naming |
| [roadmap.md](roadmap.md) | Development phases and milestones |
| [operations.md](operations.md) | Bounded operation specifications |

## Overview

Sunny provides AI agents with structured control over Ableton Live. The system combines a music theory engine with real-time transport, enabling operations from chord generation through parameter modulation.

## Architecture

```
AI Client → MCP Server → Transport → Remote Script → Ableton Live
                ↑                          ↓
           Theory Engine              State Updates
```

The theory engine provides musical intelligence. The transport layer provides real-time communication. The snapshot system provides reversibility.

## Quick Reference

```bash
make install    # Install dependencies
make test       # Run tests
make check      # Run all checks
make run        # Start the server
```
