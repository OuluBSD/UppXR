# ComputerVision Package AGENTS Guide

## Scope
- Applies to `uppsrc/ComputerVision` and its entire sub-tree of U++ packages.

## Purpose
- Computer vision algorithms and processing for U++ applications
- Provides image processing, feature detection, and machine vision capabilities
- Supports multiple camera interfaces and image formats
- Integrates with other U++ packages for vision-based applications

## Key Areas
- **Image Processing**: Core image manipulation and filtering operations
- **Feature Detection**: Edge detection, corner detection, and feature extraction
- **Object Recognition**: Object detection and classification algorithms
- **Camera Interfaces**: Support for various camera types and interfaces
- **Calibration**: Camera calibration and stereo vision support
- **Machine Learning Integration**: Interface to ML models for vision tasks

## Extension Points
- Add new image processing steps in `ImageProcessing.h`
- Implement new feature extractors following `ORB` / `BrightnessBinaryFeature`
- Add new object recognition algorithms
- Extend camera interfaces with new device support
- Integrate with external vision libraries

## Conventions
- Follow `/CODESTYLE.md` for code style and documentation
- Use proper U++ naming conventions (PascalCase for classes, camelCase for methods)
- Include `AGENTS.md` as first file in `.upp` manifest
- Document all public APIs with Topic++ (`.tpp`) style comments
- Use `Separate` and `ReadOnly` options in `.upp` for logical grouping
- Keep package header (`AGENTS.md`) up to date with any structural changes
- Use proper acronym casing for shared components
- Prefer shared implementations over duplicated code patterns
- Follow existing patterns in the codebase

- Follow repo-wide naming rules: `ai/rules/naming-conventions.md`

## Dependencies
- Core: Basic U++ functionality
- Draw: Image handling and drawing operations
- Common: Shared utilities and base classes
- TheIDE: Integration with the IDE application

## Where To Start
- Review `ComputerVision.h` — the umbrella aggregating `Cascade.h`, `Backend.h`, `ImageProcessing.h`, `ORB.h`, `BrightnessBinaryFeature.h`
- Study `Cascade` for detection cascades and `Backend` for the backend interface
- Examine `ORB` and `BrightnessBinaryFeature` for feature extraction

## Design Principles
- Efficient image processing with minimal memory copies
- Extensible architecture for new vision algorithms
- Consistent API design for vision operations
- Comprehensive error handling and reporting
- Real-time performance for vision applications

