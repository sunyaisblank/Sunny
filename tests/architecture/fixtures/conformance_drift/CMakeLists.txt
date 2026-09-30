cmake_minimum_required(VERSION 3.28)
project(SunnyConformanceDriftFixture VERSION 0.4.0 LANGUAGES NONE)

include("${SUNNY_ARCHITECTURE_MODULE}")
sunny_verify_repository_metadata(
    PROJECT_VERSION "${PROJECT_VERSION}"
    BRIDGE_PROTOCOL_VERSION 35
    SNAPSHOT_SCHEMA_VERSION 28
)
