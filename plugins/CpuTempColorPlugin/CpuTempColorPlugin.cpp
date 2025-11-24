#include "CpuTempColorPlugin.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>
#include <vector>
#include <QBoxLayout>
#include <QGridLayout>
#include <QGroupBox>

namespace fs = std::filesystem;

namespace
{
    constexpr int kDefaultPollIntervalMs = 2000;
    constexpr double kDefaultCoolC = 35.0;
    constexpr double kDefaultWarmC = 60.0;
    constexpr double kDefaultHotC  = 85.0;

    QColor LerpColor(const QColor& a, const QColor& b, double t)
    {
        t = std::clamp(t, 0.0, 1.0);
        const int r = static_cast<int>(a.red()   + (b.red()   - a.red())   * t);
        const int g = static_cast<int>(a.green() + (b.green() - a.green()) * t);
        const int bch = static_cast<int>(a.blue()  + (b.blue()  - a.blue())  * t);
        return QColor(r, g, bch);
    }
}

CpuTempColorPlugin::CpuTempColorPlugin() :
    resource_manager_(nullptr),
    widget_(nullptr),
    status_label_(nullptr),
    color_preview_(nullptr),
    cool_spin_(nullptr),
    warm_spin_(nullptr),
    hot_spin_(nullptr),
    interval_spin_(nullptr),
    timer_(nullptr),
    cool_temp_c_(kDefaultCoolC),
    warm_temp_c_(kDefaultWarmC),
    hot_temp_c_(kDefaultHotC),
    current_color_(QColor(0, 122, 255))
{
}

CpuTempColorPlugin::~CpuTempColorPlugin()
{
    Unload();
}

OpenRGBPluginInfo CpuTempColorPlugin::GetPluginInfo()
{
    OpenRGBPluginInfo info;

    info.Name        = "CPU Temperature Color";
    info.Description = "Sets all LEDs to a solid color based on CPU temperature (blue -> yellow -> red).";
    info.Version     = "0.1.0";
    info.Commit      = "local";
    info.URL         = "https://gitlab.com/OpenRGBDevelopers";
    info.Location    = OPENRGB_PLUGIN_LOCATION_SETTINGS;
    info.Label       = "CPU Temp Color";

    return info;
}

unsigned int CpuTempColorPlugin::GetPluginAPIVersion()
{
    return OPENRGB_PLUGIN_API_VERSION;
}

void CpuTempColorPlugin::Load(ResourceManagerInterface* resource_manager_ptr)
{
    resource_manager_ = resource_manager_ptr;

    if(!widget_)
    {
        BuildUi();
    }

    if(!timer_)
    {
        timer_ = new QTimer(this);
        connect(timer_, &QTimer::timeout, this, &CpuTempColorPlugin::PollTemperature);
    }

    UpdateInterval();
    PollTemperature();
}

QWidget* CpuTempColorPlugin::GetWidget()
{
    if(!widget_)
    {
        BuildUi();
    }

    return widget_;
}

QMenu* CpuTempColorPlugin::GetTrayMenu()
{
    return nullptr;
}

void CpuTempColorPlugin::Unload()
{
    if(timer_)
    {
        timer_->stop();
    }
}

void CpuTempColorPlugin::BuildUi()
{
    widget_ = new QWidget();

    auto* layout = new QVBoxLayout(widget_);
    auto* description = new QLabel("Solid color follows CPU temperature: blue when cool, yellow when warm, red when hot.");
    description->setWordWrap(true);
    layout->addWidget(description);

    auto* thresholds = new QGroupBox("Temperature thresholds");
    auto* thresholds_layout = new QGridLayout(thresholds);

    cool_spin_ = new QDoubleSpinBox();
    cool_spin_->setSuffix(" °C");
    cool_spin_->setRange(-20.0, 120.0);
    cool_spin_->setValue(cool_temp_c_);
    cool_spin_->setSingleStep(1.0);

    warm_spin_ = new QDoubleSpinBox();
    warm_spin_->setSuffix(" °C");
    warm_spin_->setRange(-10.0, 140.0);
    warm_spin_->setValue(warm_temp_c_);
    warm_spin_->setSingleStep(1.0);

    hot_spin_ = new QDoubleSpinBox();
    hot_spin_->setSuffix(" °C");
    hot_spin_->setRange(0.0, 200.0);
    hot_spin_->setValue(hot_temp_c_);
    hot_spin_->setSingleStep(1.0);

    thresholds_layout->addWidget(new QLabel("Cool"), 0, 0);
    thresholds_layout->addWidget(cool_spin_, 0, 1);
    thresholds_layout->addWidget(new QLabel("Warm"), 1, 0);
    thresholds_layout->addWidget(warm_spin_, 1, 1);
    thresholds_layout->addWidget(new QLabel("Hot"), 2, 0);
    thresholds_layout->addWidget(hot_spin_, 2, 1);

    layout->addWidget(thresholds);

    auto* interval_group = new QGroupBox("Update");
    auto* interval_layout = new QGridLayout(interval_group);

    interval_spin_ = new QSpinBox();
    interval_spin_->setSuffix(" ms");
    interval_spin_->setRange(250, 10000);
    interval_spin_->setSingleStep(250);
    interval_spin_->setValue(kDefaultPollIntervalMs);

    interval_layout->addWidget(new QLabel("Poll interval"), 0, 0);
    interval_layout->addWidget(interval_spin_, 0, 1);

    layout->addWidget(interval_group);

    status_label_ = new QLabel("Waiting for temperature data...");
    layout->addWidget(status_label_);

    color_preview_ = new QFrame();
    color_preview_->setFrameShape(QFrame::Box);
    color_preview_->setLineWidth(1);
    color_preview_->setFixedHeight(28);
    layout->addWidget(color_preview_);

    layout->addStretch(1);

    connect(interval_spin_, &QSpinBox::editingFinished, this, &CpuTempColorPlugin::UpdateInterval);
    connect(cool_spin_, &QDoubleSpinBox::editingFinished, this, &CpuTempColorPlugin::NormalizeThresholds);
    connect(warm_spin_, &QDoubleSpinBox::editingFinished, this, &CpuTempColorPlugin::NormalizeThresholds);
    connect(hot_spin_, &QDoubleSpinBox::editingFinished, this, &CpuTempColorPlugin::NormalizeThresholds);
}

void CpuTempColorPlugin::UpdateInterval()
{
    if(!timer_ || !interval_spin_)
    {
        return;
    }

    const int interval = interval_spin_->value();
    timer_->start(interval);
}

void CpuTempColorPlugin::NormalizeThresholds()
{
    if(!(cool_spin_ && warm_spin_ && hot_spin_))
    {
        return;
    }

    cool_temp_c_ = cool_spin_->value();
    warm_temp_c_ = std::max(warm_spin_->value(), cool_temp_c_ + 1.0);
    hot_temp_c_  = std::max(hot_spin_->value(), warm_temp_c_ + 1.0);

    warm_spin_->setValue(warm_temp_c_);
    hot_spin_->setValue(hot_temp_c_);
}

void CpuTempColorPlugin::PollTemperature()
{
    NormalizeThresholds();

    const std::optional<double> temperature_c = ReadCpuTemperatureC();

    if(!temperature_c.has_value())
    {
        if(status_label_)
        {
            status_label_->setText("CPU temperature unavailable.");
        }
        return;
    }

    const QColor color = ColorForTemperature(temperature_c.value());

    if(color != current_color_)
    {
        ApplyColor(color);
        current_color_ = color;
    }

    UpdateUi(temperature_c.value(), color);
}

void CpuTempColorPlugin::UpdateUi(double temperature_c, const QColor& color)
{
    if(status_label_)
    {
        status_label_->setText(QString("CPU temperature: %1 °C").arg(temperature_c, 0, 'f', 1));
    }

    if(color_preview_)
    {
        const QString style = QString("background-color: rgb(%1, %2, %3); border: 1px solid #444;")
                                  .arg(color.red())
                                  .arg(color.green())
                                  .arg(color.blue());
        color_preview_->setStyleSheet(style);
    }
}

QColor CpuTempColorPlugin::ColorForTemperature(double temperature_c) const
{
    const QColor cool_color(0, 122, 255);
    const QColor warm_color(255, 200, 0);
    const QColor hot_color(220, 24, 24);

    if(temperature_c <= cool_temp_c_)
    {
        return cool_color;
    }

    if(temperature_c >= hot_temp_c_)
    {
        return hot_color;
    }

    if(temperature_c <= warm_temp_c_)
    {
        const double t = (temperature_c - cool_temp_c_) / (warm_temp_c_ - cool_temp_c_);
        return LerpColor(cool_color, warm_color, t);
    }

    const double t = (temperature_c - warm_temp_c_) / (hot_temp_c_ - warm_temp_c_);
    return LerpColor(warm_color, hot_color, t);
}

void CpuTempColorPlugin::ApplyColor(const QColor& color)
{
    if(resource_manager_ == nullptr)
    {
        return;
    }

    const RGBColor rgb = ToRGBColor(color.red(), color.green(), color.blue());

    for(RGBController* controller : resource_manager_->GetRGBControllers())
    {
        if(controller == nullptr)
        {
            continue;
        }

        const int previous_mode = controller->GetMode();
        controller->SetCustomMode();

        if(previous_mode != controller->GetMode())
        {
            controller->UpdateMode();
        }

        controller->SetAllLEDs(rgb);
        controller->UpdateLEDs();
    }
}

std::optional<double> CpuTempColorPlugin::ReadCpuTemperatureC()
{
    if(const auto hwmon = ReadTemperatureFromHwmon())
    {
        return hwmon;
    }

#ifdef _WIN32
    if(const auto wmi = ReadTemperatureFromWmi())
    {
        return wmi;
    }
#endif

    if(const auto thermal = ReadTemperatureFromThermalZones())
    {
        return thermal;
    }

    return std::nullopt;
}

std::optional<double> CpuTempColorPlugin::ReadTemperatureFromFile(const fs::path& path) const
{
    std::ifstream stream(path);

    if(!stream.is_open())
    {
        return std::nullopt;
    }

    double value = 0.0;
    stream >> value;

    if(stream.fail())
    {
        return std::nullopt;
    }

    if(value > 200.0)
    {
        value /= 1000.0;
    }

    return value;
}

bool CpuTempColorPlugin::IsCpuLabel(const std::string& label)
{
    std::string lowered(label);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

    return lowered.find("cpu") != std::string::npos ||
           lowered.find("package") != std::string::npos ||
           lowered.find("core") != std::string::npos ||
           lowered.find("tctl") != std::string::npos;
}

std::optional<double> CpuTempColorPlugin::ReadTemperatureFromHwmon()
{
    const fs::path hwmon_root("/sys/class/hwmon");

    if(!fs::exists(hwmon_root))
    {
        return std::nullopt;
    }

    bool has_candidate = false;
    double best_value = 0.0;
    int best_score = -1;

    for(const auto& entry : fs::directory_iterator(hwmon_root))
    {
        if(!entry.is_directory())
        {
            continue;
        }

        for(const auto& file : fs::directory_iterator(entry.path()))
        {
            const std::string filename = file.path().filename().string();
            const std::size_t input_pos = filename.find("_input");

            if(filename.find("temp") != 0 || input_pos == std::string::npos)
            {
                continue;
            }

            const std::string base_name = filename.substr(0, input_pos);
            const fs::path label_path = file.path().parent_path() / (base_name + "_label");
            std::string label;

            if(fs::exists(label_path))
            {
                std::ifstream label_stream(label_path);
                if(label_stream.is_open())
                {
                    std::getline(label_stream, label);
                }
            }

            const std::optional<double> value = ReadTemperatureFromFile(file.path());

            if(!value.has_value())
            {
                continue;
            }

            const bool cpu_label = IsCpuLabel(label);
            const int score = cpu_label ? 10 : 0;

            if(!has_candidate || score > best_score)
            {
                best_score = score;
                best_value = value.value();
                has_candidate = true;
            }
        }
    }

    if(has_candidate)
    {
        return best_value;
    }

    return std::nullopt;
}

std::optional<double> CpuTempColorPlugin::ReadTemperatureFromThermalZones()
{
    const fs::path thermal_root("/sys/class/thermal");

    if(!fs::exists(thermal_root))
    {
        return std::nullopt;
    }

    std::optional<double> first_value;

    for(const auto& entry : fs::directory_iterator(thermal_root))
    {
        if(!entry.is_directory())
        {
            continue;
        }

        const fs::path type_path = entry.path() / "type";
        std::string type_label;

        if(fs::exists(type_path))
        {
            std::ifstream type_stream(type_path);
            if(type_stream.is_open())
            {
                std::getline(type_stream, type_label);
            }
        }

        const fs::path temp_path = entry.path() / "temp";
        const std::optional<double> value = ReadTemperatureFromFile(temp_path);

        if(value.has_value() && IsCpuLabel(type_label))
        {
            return value;
        }

        if(!first_value.has_value() && value.has_value())
        {
            first_value = value;
        }
    }

    return first_value;
}

#ifdef _WIN32
std::optional<double> CpuTempColorPlugin::ReadTemperatureFromWmi()
{
    return std::nullopt;
}
#endif
