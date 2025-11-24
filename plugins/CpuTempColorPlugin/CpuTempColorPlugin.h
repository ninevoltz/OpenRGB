#pragma once

#include <optional>
#include <filesystem>
#include <QColor>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QLabel>
#include <QMenu>
#include <QSpinBox>
#include <QTimer>
#include <QWidget>
#include "OpenRGBPluginInterface.h"
#include "RGBController/RGBController.h"

class CpuTempColorPlugin : public QObject, public OpenRGBPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenRGBPluginInterface_IID)
    Q_INTERFACES(OpenRGBPluginInterface)

public:
    CpuTempColorPlugin();
    ~CpuTempColorPlugin() override;

    OpenRGBPluginInfo   GetPluginInfo() override;
    unsigned int        GetPluginAPIVersion() override;

    void                Load(ResourceManagerInterface* resource_manager_ptr) override;
    QWidget*            GetWidget() override;
    QMenu*              GetTrayMenu() override;
    void                Unload() override;

private slots:
    void                PollTemperature();
    void                UpdateInterval();
    void                NormalizeThresholds();

private:
    void                BuildUi();
    void                UpdateUi(double temperature_c, const QColor& color);
    QColor              ColorForTemperature(double temperature_c) const;
    void                ApplyColor(const QColor& color);

    std::optional<double> ReadCpuTemperatureC();
    std::optional<double> ReadTemperatureFromHwmon();
    std::optional<double> ReadTemperatureFromThermalZones();
#ifdef _WIN32
    std::optional<double> ReadTemperatureFromWmi();
#endif
    std::optional<double> ReadTemperatureFromFile(const std::filesystem::path& path) const;
    static bool         IsCpuLabel(const std::string& label);

    ResourceManagerInterface* resource_manager_;
    QWidget*            widget_;
    QLabel*             status_label_;
    QFrame*             color_preview_;
    QDoubleSpinBox*     cool_spin_;
    QDoubleSpinBox*     warm_spin_;
    QDoubleSpinBox*     hot_spin_;
    QSpinBox*           interval_spin_;
    QTimer*             timer_;

    double              cool_temp_c_;
    double              warm_temp_c_;
    double              hot_temp_c_;
    QColor              current_color_;
};
