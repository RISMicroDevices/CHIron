#pragma once

#include "latency_analysis.hpp"

#include <QString>

namespace CHIron::Gui {

bool ExportLatencyDiffReportXlsx(const LatencyDiffReport& report,
                                 const QString& filePath,
                                 QString& errorText);

}  // namespace CHIron::Gui
