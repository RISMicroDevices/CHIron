# CHIron: Open-source AMBA CHI Infrastructure

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

## [Errata](ERRATA.md)

- Some differences from AMBA CHI Specification for specific issues were made to keep up with the essential engineering philosophy of AMBA CHI, by our understandings.  
- These differences was stated in the [Errata](ERRATA.md). You can revert these changes if needed, and only when you completely understand what you are doing.

## Documentations

**Sorry, no public documentation available for now :(**   
But we are going to work on this part in near future!  

-----------------------

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/RISMicroDevices/CHIron) for project preview.  

