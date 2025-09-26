# #1 Initial state UCE of ReadOnce

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

This erratum was done by the following change:

```diff cpp
--- a/chi/xact/chi_xact_state/cst_xact_read.hpp
+++ b/chi/xact/chi_xact_state/cst_xact_read.hpp
@@ -39,40 +39,40 @@ namespace CHI {
                     ReadNoSnp_I
                 };
                 // ----------------------------------------------------------------------------------------------------
-                // ReadOnce             | I, UCE        | -          | I     | CompData_UC,   | RespSepData + 
+                // ReadOnce             | I             | -          | I     | CompData_UC,   | RespSepData + 
                 //                      |               |            |       | CompData_I     | DataSepResp_UC
-                constexpr CacheStateTransition ReadOnce_I_UCE_to_I = {
-                    CacheStateTransition(I | UCE, I).TypeRead()
+                constexpr CacheStateTransition ReadOnce_I_to_I = {
+                    CacheStateTransition(I, I).TypeRead()
                         .CompData(I | UC)
                         .DataSepResp(UC)
                 };
                 //
                 constexpr std::array ReadOnce = {
-                    ReadOnce_I_UCE_to_I
+                    ReadOnce_I_to_I
                 };
                 // ----------------------------------------------------------------------------------------------------
-                // ReadOnceCleanInvalid | I, UCE        | -          | I     | CompData_UC,   | RespSepData + 
+                // ReadOnceCleanInvalid | I             | -          | I     | CompData_UC,   | RespSepData + 
                 //                      |               |            |       | CompData_I     | DataSepResp_UC
-                constexpr CacheStateTransition ReadOnceCleanInvalid_I_UCE_to_I = {
-                    CacheStateTransition(I | UCE, I).TypeRead()
+                constexpr CacheStateTransition ReadOnceCleanInvalid_I_to_I = {
+                    CacheStateTransition(I, I).TypeRead()
                         .CompData(I | UC)
                         .DataSepResp(UC)
                 };
                 //
                 constexpr std::array ReadOnceCleanInvalid = {
-                    ReadOnceCleanInvalid_I_UCE_to_I
+                    ReadOnceCleanInvalid_I_to_I
                 };
                 // ----------------------------------------------------------------------------------------------------
-                // ReadOnceMakeInvalid  | I, UCE        | -          | I     | CompData_UC,   | RespSepData + 
+                // ReadOnceMakeInvalid  | I             | -          | I     | CompData_UC,   | RespSepData + 
                 //                      |               |            |       | CompData_I     | DataSepResp_UC
-                constexpr CacheStateTransition ReadOnceMakeInvalid_I_UCE_to_I = {
-                    CacheStateTransition(I | UCE, I).TypeRead()
+                constexpr CacheStateTransition ReadOnceMakeInvalid_I_to_I = {
+                    CacheStateTransition(I, I).TypeRead()
                         .CompData(I | UC)
                         .DataSepResp(UC)
                 };
                 //
                 constexpr std::array ReadOnceMakeInvalid = {
-                    ReadOnceMakeInvalid_I_UCE_to_I
+                    ReadOnceMakeInvalid_I_to_I
                 };
                 // ----------------------------------------------------------------------------------------------------
                 // ReadClean            | I             | -          | SC     | CompData_SC    | RespSepData + 
```