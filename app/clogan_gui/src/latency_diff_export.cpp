#include "latency_diff_export.hpp"

#include <QByteArray>
#include <QFile>
#include <QSaveFile>
#include <QXmlStreamWriter>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace CHIron::Gui {

namespace {

struct ZipEntry {
    QString name;
    QByteArray data;
    std::uint32_t crc = 0;
    std::uint32_t localHeaderOffset = 0;
};

std::uint32_t crc32ForData(const QByteArray& data)
{
    static std::array<std::uint32_t, 256> table{};
    static bool initialized = false;
    if (!initialized) {
        for (std::uint32_t index = 0; index < table.size(); ++index) {
            std::uint32_t value = index;
            for (int bit = 0; bit < 8; ++bit) {
                value = (value & 1U) ? (0xEDB88320U ^ (value >> 1U)) : (value >> 1U);
            }
            table[index] = value;
        }
        initialized = true;
    }

    std::uint32_t crc = 0xFFFFFFFFU;
    for (const char byte : data) {
        crc = table[(crc ^ static_cast<unsigned char>(byte)) & 0xFFU] ^ (crc >> 8U);
    }
    return crc ^ 0xFFFFFFFFU;
}

void append16(QByteArray& data, const std::uint16_t value)
{
    data.append(static_cast<char>(value & 0xFFU));
    data.append(static_cast<char>((value >> 8U) & 0xFFU));
}

void append32(QByteArray& data, const std::uint32_t value)
{
    append16(data, static_cast<std::uint16_t>(value & 0xFFFFU));
    append16(data, static_cast<std::uint16_t>((value >> 16U) & 0xFFFFU));
}

bool appendStoredZipEntry(QByteArray& zip, ZipEntry& entry)
{
    const QByteArray nameUtf8 = entry.name.toUtf8();
    if (nameUtf8.size() > (std::numeric_limits<std::uint16_t>::max)()
        || entry.data.size() > (std::numeric_limits<std::uint32_t>::max)()
        || zip.size() > (std::numeric_limits<std::uint32_t>::max)()) {
        return false;
    }
    entry.localHeaderOffset = static_cast<std::uint32_t>(zip.size());
    entry.crc = crc32ForData(entry.data);

    append32(zip, 0x04034B50U);
    append16(zip, 20);
    append16(zip, 0x0800U);
    append16(zip, 0);
    append16(zip, 0);
    append16(zip, 0);
    append32(zip, entry.crc);
    append32(zip, static_cast<std::uint32_t>(entry.data.size()));
    append32(zip, static_cast<std::uint32_t>(entry.data.size()));
    append16(zip, static_cast<std::uint16_t>(nameUtf8.size()));
    append16(zip, 0);
    zip.append(nameUtf8);
    zip.append(entry.data);
    return true;
}

bool buildZip(std::vector<ZipEntry> entries, QByteArray& zip)
{
    zip.clear();
    for (ZipEntry& entry : entries) {
        if (!appendStoredZipEntry(zip, entry)) {
            return false;
        }
    }

    if (zip.size() > (std::numeric_limits<std::uint32_t>::max)()) {
        return false;
    }
    const std::uint32_t centralOffset = static_cast<std::uint32_t>(zip.size());

    for (const ZipEntry& entry : entries) {
        const QByteArray nameUtf8 = entry.name.toUtf8();
        append32(zip, 0x02014B50U);
        append16(zip, 20);
        append16(zip, 20);
        append16(zip, 0x0800U);
        append16(zip, 0);
        append16(zip, 0);
        append16(zip, 0);
        append32(zip, entry.crc);
        append32(zip, static_cast<std::uint32_t>(entry.data.size()));
        append32(zip, static_cast<std::uint32_t>(entry.data.size()));
        append16(zip, static_cast<std::uint16_t>(nameUtf8.size()));
        append16(zip, 0);
        append16(zip, 0);
        append16(zip, 0);
        append16(zip, 0);
        append32(zip, 0);
        append32(zip, entry.localHeaderOffset);
        zip.append(nameUtf8);
    }

    if (entries.size() > (std::numeric_limits<std::uint16_t>::max)()
        || zip.size() > (std::numeric_limits<std::uint32_t>::max)()) {
        return false;
    }
    const std::uint32_t centralSize = static_cast<std::uint32_t>(zip.size()) - centralOffset;
    append32(zip, 0x06054B50U);
    append16(zip, 0);
    append16(zip, 0);
    append16(zip, static_cast<std::uint16_t>(entries.size()));
    append16(zip, static_cast<std::uint16_t>(entries.size()));
    append32(zip, centralSize);
    append32(zip, centralOffset);
    append16(zip, 0);
    return true;
}

void writeTextCell(QXmlStreamWriter& xml, const QString& ref, const QString& text, const int style = 0)
{
    xml.writeStartElement(QStringLiteral("c"));
    xml.writeAttribute(QStringLiteral("r"), ref);
    if (style > 0) {
        xml.writeAttribute(QStringLiteral("s"), QString::number(style));
    }
    xml.writeAttribute(QStringLiteral("t"), QStringLiteral("inlineStr"));
    xml.writeStartElement(QStringLiteral("is"));
    xml.writeTextElement(QStringLiteral("t"), text);
    xml.writeEndElement();
    xml.writeEndElement();
}

void writeNumberCell(QXmlStreamWriter& xml, const QString& ref, const double value, const int style = 0)
{
    xml.writeStartElement(QStringLiteral("c"));
    xml.writeAttribute(QStringLiteral("r"), ref);
    if (style > 0) {
        xml.writeAttribute(QStringLiteral("s"), QString::number(style));
    }
    xml.writeTextElement(QStringLiteral("v"), QString::number(value, 'g', 15));
    xml.writeEndElement();
}

QString cellRef(const int column, const int row)
{
    int value = column;
    QString letters;
    while (value > 0) {
        --value;
        letters.prepend(QChar(QLatin1Char('A' + (value % 26))));
        value /= 26;
    }
    return letters + QString::number(row);
}

QByteArray xmlHeader()
{
    return QByteArrayLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
}

QByteArray contentTypesXml()
{
    return xmlHeader() + QByteArrayLiteral(
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>"
        "<Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>"
        "<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>"
        "<Override PartName=\"/xl/worksheets/sheet2.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>"
        "</Types>");
}

QByteArray rootRelsXml()
{
    return xmlHeader() + QByteArrayLiteral(
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>"
        "</Relationships>");
}

QByteArray workbookRelsXml()
{
    return xmlHeader() + QByteArrayLiteral(
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>"
        "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet2.xml\"/>"
        "<Relationship Id=\"rId3\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>"
        "</Relationships>");
}

QByteArray workbookXml()
{
    return xmlHeader() + QByteArrayLiteral(
        "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
        "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
        "<sheets>"
        "<sheet name=\"Summary\" sheetId=\"1\" r:id=\"rId1\"/>"
        "<sheet name=\"Latency Diff\" sheetId=\"2\" r:id=\"rId2\"/>"
        "</sheets>"
        "</workbook>");
}

QByteArray stylesXml()
{
    return xmlHeader() + QByteArrayLiteral(
        "<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
        "<fonts count=\"2\">"
        "<font><sz val=\"11\"/><name val=\"Calibri\"/></font>"
        "<font><b/><sz val=\"11\"/><name val=\"Calibri\"/></font>"
        "</fonts>"
        "<fills count=\"2\"><fill><patternFill patternType=\"none\"/></fill><fill><patternFill patternType=\"gray125\"/></fill></fills>"
        "<borders count=\"1\"><border><left/><right/><top/><bottom/><diagonal/></border></borders>"
        "<cellStyleXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/></cellStyleXfs>"
        "<cellXfs count=\"3\">"
        "<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"/>"
        "<xf numFmtId=\"0\" fontId=\"1\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyFont=\"1\"/>"
        "<xf numFmtId=\"10\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyNumberFormat=\"1\"/>"
        "</cellXfs>"
        "<cellStyles count=\"1\"><cellStyle name=\"Normal\" xfId=\"0\" builtinId=\"0\"/></cellStyles>"
        "</styleSheet>");
}

QByteArray summarySheetXml(const LatencyDiffReport& report)
{
    QByteArray data;
    QXmlStreamWriter xml(&data);
    xml.setAutoFormatting(false);
    xml.writeStartDocument(QStringLiteral("1.0"), true);
    xml.writeStartElement(QStringLiteral("worksheet"));
    xml.writeDefaultNamespace(QStringLiteral("http://schemas.openxmlformats.org/spreadsheetml/2006/main"));
    xml.writeStartElement(QStringLiteral("sheetData"));

    const std::vector<std::pair<QString, QString>> rows = {
        {QStringLiteral("Session A"), report.sessionALabel},
        {QStringLiteral("Session B"), report.sessionBLabel},
        {QStringLiteral("Formula"), QStringLiteral("(B - A) / A")},
        {QStringLiteral("Rows"), QString::number(static_cast<qulonglong>(report.rows.size()))},
        {QStringLiteral("Samples A"), QString::number(static_cast<qulonglong>(report.analysisA.sampleCount))},
        {QStringLiteral("Samples B"), QString::number(static_cast<qulonglong>(report.analysisB.sampleCount))},
    };

    for (int rowIndex = 0; rowIndex < static_cast<int>(rows.size()); ++rowIndex) {
        const int row = rowIndex + 1;
        xml.writeStartElement(QStringLiteral("row"));
        xml.writeAttribute(QStringLiteral("r"), QString::number(row));
        writeTextCell(xml, cellRef(1, row), rows[static_cast<std::size_t>(rowIndex)].first, 1);
        writeTextCell(xml, cellRef(2, row), rows[static_cast<std::size_t>(rowIndex)].second);
        xml.writeEndElement();
    }

    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndDocument();
    return data;
}

QByteArray diffSheetXml(const LatencyDiffReport& report)
{
    QByteArray data;
    QXmlStreamWriter xml(&data);
    xml.setAutoFormatting(false);
    xml.writeStartDocument(QStringLiteral("1.0"), true);
    xml.writeStartElement(QStringLiteral("worksheet"));
    xml.writeDefaultNamespace(QStringLiteral("http://schemas.openxmlformats.org/spreadsheetml/2006/main"));

    xml.writeStartElement(QStringLiteral("sheetData"));
    xml.writeStartElement(QStringLiteral("row"));
    xml.writeAttribute(QStringLiteral("r"), QStringLiteral("1"));
    const QStringList headers = {
        QStringLiteral("Bucket"),
        QStringLiteral("Depth"),
        QStringLiteral("Metric"),
        QStringLiteral("A"),
        QStringLiteral("B"),
        QStringLiteral("Delta"),
        QStringLiteral("% Diff"),
        QStringLiteral("In A"),
        QStringLiteral("In B"),
    };
    for (int column = 0; column < headers.size(); ++column) {
        writeTextCell(xml, cellRef(column + 1, 1), headers[column], 1);
    }
    xml.writeEndElement();

    int excelRow = 2;
    for (const LatencyDiffRow& row : report.rows) {
        for (const LatencyDiffMetric& metric : row.metrics) {
            xml.writeStartElement(QStringLiteral("row"));
            xml.writeAttribute(QStringLiteral("r"), QString::number(excelRow));
            writeTextCell(xml, cellRef(1, excelRow), row.path.join(QLatin1Char('/')));
            writeNumberCell(xml, cellRef(2, excelRow), row.depth);
            writeTextCell(xml, cellRef(3, excelRow), metric.name);
            writeNumberCell(xml, cellRef(4, excelRow), metric.aValue);
            writeNumberCell(xml, cellRef(5, excelRow), metric.bValue);
            writeNumberCell(xml, cellRef(6, excelRow), metric.delta);
            if (metric.percentComparable) {
                writeNumberCell(xml, cellRef(7, excelRow), metric.percent, 2);
            } else {
                writeTextCell(xml, cellRef(7, excelRow), QStringLiteral("N/A"));
            }
            writeTextCell(xml, cellRef(8, excelRow), row.presentInA ? QStringLiteral("Yes") : QStringLiteral("No"));
            writeTextCell(xml, cellRef(9, excelRow), row.presentInB ? QStringLiteral("Yes") : QStringLiteral("No"));
            xml.writeEndElement();
            ++excelRow;
        }
    }
    xml.writeEndElement();

    if (excelRow > 2) {
        xml.writeStartElement(QStringLiteral("conditionalFormatting"));
        xml.writeAttribute(QStringLiteral("sqref"), QStringLiteral("G2:G%1").arg(excelRow - 1));
        xml.writeStartElement(QStringLiteral("cfRule"));
        xml.writeAttribute(QStringLiteral("type"), QStringLiteral("dataBar"));
        xml.writeAttribute(QStringLiteral("priority"), QStringLiteral("1"));
        xml.writeStartElement(QStringLiteral("dataBar"));
        xml.writeAttribute(QStringLiteral("showValue"), QStringLiteral("1"));
        xml.writeStartElement(QStringLiteral("cfvo"));
        xml.writeAttribute(QStringLiteral("type"), QStringLiteral("min"));
        xml.writeEndElement();
        xml.writeStartElement(QStringLiteral("cfvo"));
        xml.writeAttribute(QStringLiteral("type"), QStringLiteral("max"));
        xml.writeEndElement();
        xml.writeEmptyElement(QStringLiteral("color"));
        xml.writeAttribute(QStringLiteral("rgb"), QStringLiteral("FF63C384"));
        xml.writeEndElement();
        xml.writeEndElement();
        xml.writeEndElement();
    }

    xml.writeEndElement();
    xml.writeEndDocument();
    return data;
}

}  // namespace

bool ExportLatencyDiffReportXlsx(const LatencyDiffReport& report,
                                 const QString& filePath,
                                 QString& errorText)
{
    errorText.clear();
    if (filePath.trimmed().isEmpty()) {
        errorText = QStringLiteral("No export path was selected.");
        return false;
    }

    std::vector<ZipEntry> entries = {
        {QStringLiteral("[Content_Types].xml"), contentTypesXml()},
        {QStringLiteral("_rels/.rels"), rootRelsXml()},
        {QStringLiteral("xl/workbook.xml"), workbookXml()},
        {QStringLiteral("xl/_rels/workbook.xml.rels"), workbookRelsXml()},
        {QStringLiteral("xl/styles.xml"), stylesXml()},
        {QStringLiteral("xl/worksheets/sheet1.xml"), summarySheetXml(report)},
        {QStringLiteral("xl/worksheets/sheet2.xml"), diffSheetXml(report)},
    };

    QByteArray zip;
    if (!buildZip(std::move(entries), zip)) {
        errorText = QStringLiteral("The Latency Diff workbook is too large to export.");
        return false;
    }

    QSaveFile output(filePath);
    if (!output.open(QIODevice::WriteOnly)) {
        errorText = QStringLiteral("Cannot open %1 for writing.").arg(filePath);
        return false;
    }
    if (output.write(zip) != zip.size()) {
        errorText = QStringLiteral("Failed while writing %1.").arg(filePath);
        return false;
    }
    if (!output.commit()) {
        errorText = QStringLiteral("Failed to commit %1.").arg(filePath);
        return false;
    }
    return true;
}

}  // namespace CHIron::Gui
