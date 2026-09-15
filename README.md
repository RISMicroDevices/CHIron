# CHIron: Open-source AMBA CHI Infrastructure

[![Assisted by Kimi K3](https://img.shields.io/badge/Assisted_by-Kimi_K3-1d1d1f?style=flat&labelColor=1783FF&logo=data:image/svg%2bxml;base64,PHN2ZyB3aWR0aD0iMjQiIGhlaWdodD0iMjUiIHZpZXdCb3g9IjAgMCAyNCAyNSIgZmlsbD0ibm9uZSIgeG1sbnM9Imh0dHA6Ly93d3cudzMub3JnLzIwMDAvc3ZnIj4KPHBhdGggZD0iTTIxLjcyMDIgMC45Mzk5NDFDMjIuOTUwMiAwLjkzOTk0MSAyMy45NTAyIDEuOTM5OTQgMjMuOTUwMiAzLjE2OTk0QzIzLjk1MDIgNC4zOTk5NCAyMi45NTAyIDUuMzk5OTQgMjEuNzIwMiA1LjM5OTk0SDE5Ljc1MDJDMTkuNjAwMiA1LjM5OTk0IDE5LjQ5MDIgNS4yNzk5NCAxOS40OTAyIDUuMTM5OTRWMy4xNjk5NEMxOS40OTAyIDEuOTM5OTQgMjAuNDkwMiAwLjkzOTk0MSAyMS43MjAyIDAuOTM5OTQxWiIgZmlsbD0id2hpdGUiLz4KPHBhdGggZD0iTTkuMzkgMTMuOTUwMUwxNy44MiA1LjU5MDEyQzE3Ljk4IDUuNDMwMTIgMTcuODkgNS4xMjAxMiAxNy42OCA1LjEyMDEySDEzLjE0QzEzLjE0IDUuMTIwMTIgMTMuMDQgNS4xNDAxMiAxMyA1LjE4MDEyTDMuOTIgMTQuMTkwMUMzLjc4IDE0LjMzMDEgMy41NyAxNC4yMTAxIDMuNTcgMTMuOTgwMVY1LjM5MDEyQzMuNTcgNS4yNDAxMiAzLjQ3IDUuMTIwMTIgMy4zNSA1LjEyMDEySDAuMjE5OTk5QzAuMDk5OTk5MyA1LjEyMDEyIDAgNS4yNDAxMiAwIDUuMzkwMTJWMjMuOTIwMUMwIDI0LjA3MDEgMC4wOTk5OTkzIDI0LjE5MDEgMC4yMTk5OTkgMjQuMTkwMUgzLjM1QzMuNDcgMjQuMTkwMSAzLjU3IDI0LjA3MDEgMy41NyAyMy45MjAxVjIwLjE0MDFDMy41NyAyMC4wNjAxIDMuNiAxOS45ODAxIDMuNjUgMTkuOTMwMUw2LjQ3IDE3LjE0MDFDNi41NCAxNy4wNzAxIDYuNjMgMTcuMDYwMSA2LjcxIDE3LjExMDFMMTQuMjQgMjIuNjUwMUMxNS40NyAyMy40ODAxIDE2Ljg1IDIzLjk5MDEgMTguMjUgMjQuMTQwMUMxOC4zNyAyNC4xNTAxIDE4LjQ4IDI0LjAzMDEgMTguNDggMjMuODcwMVYyMC4zMTAxQzE4LjQ4IDIwLjE3MDEgMTguNCAyMC4wNjAxIDE4LjI5IDIwLjA1MDFDMTcuNDcgMTkuOTIwMSAxNi42NiAxOS42MDAxIDE1Ljk0IDE5LjExMDFMOS40MiAxNC4zOTAxQzkuMjggMTQuMzAwMSA5LjI3IDE0LjA3MDEgOS4zOSAxMy45NTAxWiIgZmlsbD0id2hpdGUiLz4KPC9zdmc%2BCg%3D%3D)](https://www.kimi.com/) [![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/RISMicroDevices/CHIron)

## Summary

> World's first open-source AMBA CHI toolset

- **Currently serving or served XiangShan in-house development** ([About OpenXiangShan](https://github.com/OpenXiangShan))  
- Currently mainly supporting AMBA CHI Issue E, with basic support and future plan for Issue B/C/G  
- Constructing complete protocol level abstraction  
- Completing transaction level abstraction  
    - Fully covered demands of XiangShan Kunminghu V2  
- Designed to be infrastructure of infrastructures  
    - Aimed at supporting prototyping, testing, verification and profiling demands 
    - All codes were designed to be API, feel free to call or modify  
    - Possible to be kernel or UVMs, but no longer stuck on UVM platforms  
    - **Freedom to use in open-source projects**  

## Known Issues

- **MTE not supported**, and not on the near-future roadmap
- **DVM not supported**, and not on the near-future roadmap
- **Xaction (Transaction Modeling) now only supports Issue E**
- **Exclusive Monitor tracking not supported**
- **SnpPreferUnique & SnpPreferUniqueFwd under exclusive sequence not fully supported**
- **WriteDataCancel** needs more detailed modeling and checkers

## [Errata](ERRATA.md)

- Some differences from AMBA CHI Specification for specific issues were made to keep up with the essential engineering philosophy of AMBA CHI, by our understandings.  
- These differences was stated in the [Errata](ERRATA.md). You can revert these changes if needed, and only when you completely understand what you are doing.

## Documentations

**Sorry, no public documentation available for now :(**   
But we are going to work on this part in near future!  

-----------------------

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/RISMicroDevices/CHIron) for project preview.  

