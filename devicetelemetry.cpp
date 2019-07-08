/*
 * Copyright 2018 Azlehria
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "devicetelemetry.h"
#include "platforms.h"
#include "utils.h"
#include "DynamicLibs/dlnvml.h"
#include "DynamicLibs/dlopencl.h"
#include "DynamicLibs/dlcuda.h"

#include <memory>
#include <functional>
#include <array>
#include <string_view>
#include <type_traits>

using namespace std::literals;
using namespace Nabiki::Utils;

template auto Nabiki::MakeDeviceTelemetryObject( cl_device_id const& ) -> std::unique_ptr<IDeviceTelemetry>;
template auto Nabiki::MakeDeviceTelemetryObject( CUdevice const& ) -> std::unique_ptr<IDeviceTelemetry>;
template auto Nabiki::MakeDeviceTelemetryObject( nullptr_t const& ) -> std::unique_ptr<IDeviceTelemetry>;

IDeviceTelemetry::~IDeviceTelemetry() {}

namespace Nabiki
{
  class OpenClDeviceTelemetry : public IDeviceTelemetry
  {
  public:
    OpenClDeviceTelemetry( cl_device_id const& device );
    ~OpenClDeviceTelemetry() = default;
    
    auto getName() -> std::string const& final;
    auto getClockMem() -> uint32_t const final;
    auto getClockCore() -> uint32_t const final;
    auto getPowerWatts() -> uint32_t const final;
    auto getFanSpeed() -> uint32_t const final;
    auto getTemperature() -> uint32_t const final;
    auto getDeviceType() -> device_type_t const final;

  private:
    OpenClDeviceTelemetry() = delete;

    std::string m_name;

    cl_device_id device_;
    Opencl cl_;
  };
  
  class NvmlDeviceTelemetry : public IDeviceTelemetry
  {
  public:
    NvmlDeviceTelemetry( nvmlDevice_t const& device );
    ~NvmlDeviceTelemetry();
    
    auto getName() -> std::string const& final;
    auto getClockMem() -> uint32_t const final;
    auto getClockCore() -> uint32_t const final;
    auto getPowerWatts() -> uint32_t const final;
    auto getFanSpeed() -> uint32_t const final;
    auto getTemperature() -> uint32_t const final;
    auto getDeviceType() -> device_type_t const final;

  private:
    NvmlDeviceTelemetry() = delete;

    std::string m_name;

    nvmlDevice_t device_;
    Nvml nvml_;
  };
  
  class CpuDeviceTelemetry : public IDeviceTelemetry
  {
  public:
    CpuDeviceTelemetry();
    ~CpuDeviceTelemetry() = default;
    
    auto getName() -> std::string const& final;
    auto getClockMem() -> uint32_t const final;
    auto getClockCore() -> uint32_t const final;
    auto getPowerWatts() -> uint32_t const final;
    auto getFanSpeed() -> uint32_t const final;
    auto getTemperature() -> uint32_t const final;
    auto getDeviceType() -> device_type_t const final;

  private:
    std::string m_name;
  };

  template<typename T>
  auto MakeDeviceTelemetryObject( [[maybe_unused]] T const& device ) -> std::unique_ptr<IDeviceTelemetry>
  {
    static_assert( std::is_same_v<std::decay_t<T>, cl_device_id> ||
                   std::is_same_v<std::decay_t<T>, CUdevice> ||
                   std::is_same_v<std::decay_t<T>, nullptr_t> ||
                   std::is_same_v<std::decay_t<T>, void>,
                   "Type must be cl::Device or nvmlDevice_t or nullptr_t or void" );

    if constexpr( std::is_same_v<std::decay_t<T>, nullptr_t> ||
                  std::is_same_v<std::decay_t<T>, void> )
    {
      return std::move( std::make_unique<CpuDeviceTelemetry>() );
    }

    if constexpr( std::is_same_v<std::decay_t<T>, cl_device_id> ||
                  std::is_same_v<std::decay_t<T>, CUdevice> )
    {
      if constexpr( std::is_same_v<std::decay_t<T>, cl_device_id> )
      {
        Opencl cl{};

        std::string plat_name;
        cl_platform_id plat{};
        cl.GetDeviceInfo( device, CL_DEVICE_PLATFORM, sizeof( plat ), &plat, nullptr );
        {
          size_t name_size{ 0 };
          cl.GetPlatformInfo( plat, CL_PLATFORM_NAME, 0u, nullptr, &name_size );
          plat_name.resize( name_size );
          cl.GetPlatformInfo( plat, CL_PLATFORM_NAME, name_size, plat_name.data(), nullptr );
        }

        if( plat_name.find( "NVIDIA"s ) == std::string::npos )
        {
          return std::move( std::make_unique<OpenClDeviceTelemetry>( device ) );
        }
      }

      nvmlDevice_t nvml_handle_;
      Nvml nvml{};
      nvml.Init();

      char busId[NVML_DEVICE_PCI_BUS_ID_BUFFER_SIZE];

      if constexpr( std::is_same_v<std::decay_t<T>, cl_device_id> )
      {
        Opencl cl{};

        cl_uint clBusId, clSlotId;
        cl.GetDeviceInfo( device, CL_DEVICE_PCI_BUS_ID_NV, sizeof( cl_uint ), &clBusId, nullptr );
        cl.GetDeviceInfo( device, CL_DEVICE_PCI_SLOT_ID_NV, sizeof( cl_uint ), &clSlotId, nullptr );

        snprintf( busId, NVML_DEVICE_PCI_BUS_ID_BUFFER_SIZE, NVML_DEVICE_PCI_BUS_ID_FMT, 0, clBusId, clSlotId );
      }
      else
      {
        Cuda cu{};

        int32_t cudaBusId, cudaSlotId;
        cu.DeviceGetAttribute( &cudaBusId, CU_DEVICE_ATTRIBUTE_PCI_BUS_ID, device );
        cu.DeviceGetAttribute( &cudaSlotId, CU_DEVICE_ATTRIBUTE_PCI_DEVICE_ID, device );

        snprintf( busId, NVML_DEVICE_PCI_BUS_ID_BUFFER_SIZE, NVML_DEVICE_PCI_BUS_ID_FMT, 0, cudaBusId, cudaSlotId );
      }

      nvml.DeviceGetHandleByPciBusId( busId, &nvml_handle_ );

      return std::move( std::make_unique<NvmlDeviceTelemetry>( nvml_handle_ ) );
    }
  }

  OpenClDeviceTelemetry::OpenClDeviceTelemetry( cl_device_id const& device ) :
    device_( device ),
    cl_()
  {
    {
      size_t name_size{ 0 };
      cl_.GetDeviceInfo( device_, CL_DEVICE_NAME, 0u, nullptr, &name_size );
      m_name.resize( name_size );
      cl_.GetDeviceInfo( device_, CL_DEVICE_NAME, name_size, m_name.data(), nullptr );
    }

    replaceSubstring( m_name, "(R)"sv, ""sv );
    replaceSubstring( m_name, "(tm)"sv, ""sv );
    replaceSubstring( m_name, "(TM)"sv, ""sv );
    replaceSubstring( m_name, "CPU"sv, ""sv );
    replaceSubstring( m_name, "Eight-Core"sv, ""sv );
    replaceSubstring( m_name, "Six-Core"sv, ""sv );
    replaceSubstring( m_name, "Quad-Core"sv, ""sv );
    replaceSubstring( m_name, "Processor"sv, ""sv );
    replaceSubstring( m_name, "  "sv, " "sv );
    size_t end{ m_name.find_first_of( '\0' ) }, t_end{ 0 };
    if( (t_end = m_name.find_first_of( "@"s ) - 1) < end )
      end = t_end;
    if( (t_end = m_name.find_last_not_of( " \t"s ), end) < end )
      end = t_end;
    m_name = m_name.substr( m_name.find_first_not_of( " \t"s ), end );
  }

  auto OpenClDeviceTelemetry::getName() -> std::string const&
  {
    return m_name;
  }

  auto OpenClDeviceTelemetry::getClockMem() -> uint32_t const
  {
    return 0;
  }

  auto OpenClDeviceTelemetry::getClockCore() -> uint32_t const
  {
    return 0;
  }

  auto OpenClDeviceTelemetry::getPowerWatts() -> uint32_t const
  {
    return 0;
  }

  auto OpenClDeviceTelemetry::getFanSpeed() -> uint32_t const
  {
    return 0;
  }

  auto OpenClDeviceTelemetry::getTemperature() -> uint32_t const
  {
    return 0;
  }

  auto OpenClDeviceTelemetry::getDeviceType() -> device_type_t const
  {
    return DEVICE_OPENCL;
  }

  NvmlDeviceTelemetry::NvmlDeviceTelemetry( nvmlDevice_t const& device ) :
    device_( device ),
    nvml_()
  {
    char t_name[NVML_DEVICE_NAME_BUFFER_SIZE];
    nvml_.DeviceGetName( device_, t_name, NVML_DEVICE_NAME_BUFFER_SIZE );
    m_name = t_name;
  }

  NvmlDeviceTelemetry::~NvmlDeviceTelemetry()
  {
    nvml_.Shutdown();
  }

  auto NvmlDeviceTelemetry::getName() -> std::string const&
  {
    return m_name;
  }

  auto NvmlDeviceTelemetry::getClockMem() -> uint32_t const
  {
    uint32_t t_memory;
    nvml_.DeviceGetClockInfo( device_, NVML_CLOCK_MEM, &t_memory );
    return t_memory;
  }

  auto NvmlDeviceTelemetry::getClockCore() -> uint32_t const
  {
    uint32_t t_core;
    nvml_.DeviceGetClockInfo( device_, NVML_CLOCK_GRAPHICS, &t_core );
    return t_core;
  }

  auto NvmlDeviceTelemetry::getPowerWatts() -> uint32_t const
  {
    uint32_t t_power;
    nvml_.DeviceGetPowerUsage( device_, &t_power );
    return t_power / 1000;
  }

  auto NvmlDeviceTelemetry::getFanSpeed() -> uint32_t const
  {
    uint32_t t_fan;
    nvml_.DeviceGetFanSpeed( device_, &t_fan );
    return t_fan;
  }

  auto NvmlDeviceTelemetry::getTemperature() -> uint32_t const
  {
    uint32_t t_temp;
    nvml_.DeviceGetTemperature( device_, NVML_TEMPERATURE_GPU, &t_temp );
    return t_temp;
  }

  auto NvmlDeviceTelemetry::getDeviceType() -> device_type_t const
  {
    return DEVICE_CUDA;
  }

  CpuDeviceTelemetry::CpuDeviceTelemetry()
    : m_name( GetRawCpuName() )
  {
    replaceSubstring( m_name, "(R)"sv, ""sv );
    replaceSubstring( m_name, "(tm)"sv, ""sv );
    replaceSubstring( m_name, "(TM)"sv, ""sv );
    replaceSubstring( m_name, "CPU"sv, ""sv );
    replaceSubstring( m_name, "Eight-Core"sv, ""sv );
    replaceSubstring( m_name, "Six-Core"sv, ""sv );
    replaceSubstring( m_name, "Quad-Core"sv, ""sv );
    replaceSubstring( m_name, "Processor"sv, ""sv );
    replaceSubstring( m_name, "  "sv, " "sv );
    size_t end{ m_name.find_first_of( '\0' ) }, t_end{ 0 };
    if( (t_end = m_name.find_first_of( "@"s ) - 1) < end )
      end = t_end;
    if( (t_end = m_name.find_last_not_of( " \t"s )) < end )
      end = t_end;
    m_name = m_name.substr( m_name.find_first_not_of( " \t"s ), end );
  }

  auto CpuDeviceTelemetry::getName() -> std::string const&
  {
    return m_name;
  }

  auto CpuDeviceTelemetry::getClockMem() -> uint32_t const
  {
    return 0;
  }

  auto CpuDeviceTelemetry::getClockCore() -> uint32_t const
  {
    return 0;
  }

  auto CpuDeviceTelemetry::getPowerWatts() -> uint32_t const
  {
    return 0;
  }

  auto CpuDeviceTelemetry::getFanSpeed() -> uint32_t const
  {
    return 0;
  }

  auto CpuDeviceTelemetry::getTemperature() -> uint32_t const
  {
    return 0;
  }

  auto CpuDeviceTelemetry::getDeviceType() -> device_type_t const
  {
    return DEVICE_CPU;
  }
}
