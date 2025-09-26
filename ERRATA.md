# 1. Initial state UCE of ReadOnce

## 1.1 Background

In AMBA Specification Issue E.c and eariler, **initial state UCE** (at the sending of Read request) **was allowed for ReadOnce** (also ReadOnceCleanInvalid, ReadOnceMakeInvalid).   
But UCE (or any state with Unique) obtains the **possible silent transition of Unique Clean to Unique Dirty**, which violates the initial state constraint of ReadOnce.  
This rule was "corrected" in AMBA CHI Specification Issue G, constrainting the initial state of ReadOnce to only I.

## 1.2 Erratum

In AMBA CHI Specification Issue E.c and earlier issues:

| Request | UD | UC | SD | SC | I | UDP | UCE |
|-----------|---|---|---|---|---|---|---|
| ReadOnce | - | - | - | - | Y | - | **Y** |
| ReadOnceCleanInvalid | - | - | - | - | Y | - | **Y** |
| ReadOnceMakeInvalid | - | - | - | - | Y | - | **Y** |
> Table. Permitted Requester cache state at the sending on Read request

In AMBA CHI Specification Issue G:

| Request | UD | UC | SD | SC | I | UDP | UCE |
|-----------|---|---|---|---|---|---|---|
| ReadOnce | - | - | - | - | Y | - | **-** |
| ReadOnceCleanInvalid | - | - | - | - | Y | - | **-** |
| ReadOnceMakeInvalid | - | - | - | - | Y | - | **-** |
> Table. Permitted Requester cache state at the sending on Read request

In CHIron, **we always obey the specification of Issue G** for any version of issue configuration.
