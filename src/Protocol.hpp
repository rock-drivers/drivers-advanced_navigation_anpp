#ifndef ADVANCED_NAVIGATION_ANPP_HEADER_HPP
#define ADVANCED_NAVIGATION_ANPP_HEADER_HPP

#include <cstdint>
#include <algorithm>
#include <stdexcept>
#include <cstring>
#include <map>
#include <type_traits>

namespace advanced_navigation_anpp
{
    /** Implementation of the ANPP protocol itself
     *
     * A iodrivers_base driver that uses this protocol is provided as Driver
     *
     * The general structure is that there is one struct per packet. Thse
     * structs represent the fixed-size part of the packet - some packets have
     * variable sizes. Structs that are meant to be sent to the device have a
     * marshal() method that returns the packet payload as a set of bytes.
     * Structs that are meant to be received have an unmarshal() static method
     * that return the struct from the payload data.
     */
    namespace protocol
    {
        /** Helper function to read a 64 bit value from a byte stream */
        template<typename T, typename InputIterator>
        inline T read16(InputIterator it)
        {
            static_assert(sizeof(T) == 2, "return type is not a 2-byte data type");
            uint16_t b0 = *it;
            uint16_t b1 = *(++it);
            uint16_t r = b0 | b1 << 8;
            return reinterpret_cast<T const&>(r);
        }

        /** Helper function to read a 64 bit value from a byte stream */
        template<typename T, typename InputIterator>
        inline T read32(InputIterator it)
        {
            static_assert(sizeof(T) == 4, "return type is not a 4-byte data type");
            uint32_t b0 = *it;
            uint32_t b1 = *(++it);
            uint32_t b2 = *(++it);
            uint32_t b3 = *(++it);
            uint32_t r = b0 | b1 << 8 | b2 << 16 | b3 << 24;
            return reinterpret_cast<T const&>(r);
        }

        /** Helper function to read a 32 bit value from a byte stream */
        template<typename T, typename InputIterator>
        inline T read64(InputIterator it)
        {
            static_assert(sizeof(T) == 8, "return type is not a 8-byte data type");
            uint64_t b0 = *it;
            uint64_t b1 = *(++it);
            uint64_t b2 = *(++it);
            uint64_t b3 = *(++it);
            uint64_t b4 = *(++it);
            uint64_t b5 = *(++it);
            uint64_t b6 = *(++it);
            uint64_t b7 = *(++it);
            uint64_t r = b0 | b1 << 8 | b2 << 16 | b3 << 24 | b4 << 32 | b5 << 40 | b6 << 48 | b7 << 56;
            return reinterpret_cast<T const&>(r);
        }

        /** Helper function to write a 16 bit value into a byte stream */
        template<typename T, typename Out>
        inline void write16(Out out, T sample)
        {
            static_assert(sizeof(T) == 2, "sample is not a 2-byte data type");
            uint16_t value = reinterpret_cast<uint16_t&>(sample);
            out[0] = value & 0xFF;
            out[1] = (value >> 8) & 0xFF;
        }

        /** Helper function to read a 64 bit value from a byte stream */
        template<typename T, typename Out>
        inline void write32(Out out, T sample)
        {
            static_assert(sizeof(T) == 4, "sample is not a 4-byte data type");
            uint32_t value = reinterpret_cast<uint32_t&>(sample);
            out[0] = (value >> 0) & 0xFF;
            out[1] = (value >> 8) & 0xFF;
            out[2] = (value >> 16) & 0xFF;
            out[3] = (value >> 24) & 0xFF;
        }

        /** Helper function to read a 32 bit value from a byte stream */
        template<typename T, typename Out>
        inline void write64(Out out, T sample)
        {
            static_assert(sizeof(T) == 8, "sample is not a 8-byte data type");
            uint64_t value = reinterpret_cast<uint64_t&>(sample);
            out[1] = (value >> 8) & 0xFF;
            out[2] = (value >> 16) & 0xFF;
            out[3] = (value >> 24) & 0xFF;
            out[4] = (value >> 32) & 0xFF;
            out[5] = (value >> 40) & 0xFF;
            out[6] = (value >> 48) & 0xFF;
            out[7] = (value >> 56) & 0xFF;
        }

        /** ID of packets in the protocol */
        enum PACKET_IDS
        {
            ID_ACK                                = 0,
            ID_REQUEST                            = 1,
            ID_BOOT_MODE                          = 2,
            ID_DEVICE_INFO                        = 3,
            ID_RESTORE_FACTORY_SETTINGS           = 4,
            ID_RESET                              = 5,

            ID_SYSTEM_STATE                       = 20,
            ID_UNIX_TIME                          = 21,
            ID_STATUS                             = 23,
            ID_POSITION_STD_DEV                   = 24,
            ID_VELOCITY_STD_DEV                   = 25,
            ID_QUATERNION_STD_DEV                 = 27,
            ID_RAW_SENSORS                        = 28,
            ID_RAW_GNSS                           = 29,
            ID_SATELLITES                         = 30,
            ID_POSITION_GEODETIC                  = 31,
            ID_VELOCITY_NED                       = 35,
            ID_VELOCITY_BODY                      = 36,
            ID_ACCELERATION_BODY                  = 37,
            ID_ORIENTATION_QUATERNION             = 40,
            ID_VELOCITY_ANGULAR                   = 42,
            ID_ACCELERATION_ANGULAR               = 43,
            ID_LOCAL_MAGNETIC_FIELD               = 50,
            ID_GEOID_HEIGHT                       = 54,

            ID_PACKET_TIMER_PERIOD                = 180,
            ID_PACKETS_PERIOD                     = 181,
            ID_BAUD_RATES                         = 182,
            ID_INSTALLATION_ALIGNMENT             = 185,
            ID_FILTER_BASIC_OPTIONS               = 186,
            ID_FILTER_ADVANCED_OPTIONS            = 187,
            ID_MAGNETIC_CALIBRATION_VALUES        = 189,
            ID_MAGNETIC_CALIBRATION_CONFIGURATION = 190,
            ID_MAGNETIC_CALIBRATION_STATUS        = 191
        };

        /** Generic packet header
         *
         * The data in the packet is stored in little endian byte order
         */
        struct Header
        {
            static constexpr int SIZE = 5;

            /** Longitudinal redundancy check for the header
             *
             * <code>
             * LRC = ((packet_id + packet_length + crc[0] + crc[1])^0xFF) + 1
             * </code>
             */
            uint8_t header_checksum;
            uint8_t packet_id;
            uint8_t payload_length;
            /** Least significant byte of a crc-CCITT with starting value 0xFFFF
             * calculated over the packet data only
             */
            uint8_t payload_checksum_lsb;
            /** Most significant byte of a crc-CCITT with starting value 0xFFFF
             * calculated over the packet data only
             */
            uint8_t payload_checksum_msb;

            /** Construct an uninitialized header
             *
             * The header and payload checksums won't validate
             */
            Header();

            /** Initialize a header by filling all the fields based on data in
             * the packet
             */
            Header(uint8_t packet_id, uint8_t const* begin, uint8_t const* end);

            /** Check if the data in the header validates the header checksum
             */
            bool isValid() const;

            /** Check if the data in the header validates the packet data
             */
            bool isPacketValid(uint8_t const* begin, uint8_t const* end) const;

            /** Compute the checksum of the header
             */
            uint8_t computeHeaderChecksum() const;
        } __attribute__((packed));

        /** The maximum packet size, header included
         */
        static const int MAX_PACKET_SIZE = 256 + sizeof(Header);

        /** Compute the CRC as expected by the protocol
         */
        uint16_t crc(uint8_t const* begin, uint8_t const* end);

        /** Acknowledge result codes */
        enum ACK_RESULTS
        {
            ACK_SUCCESS                       = 0,
            ACK_FAILED_PACKET_VALIDATION_CRC  = 1,
            ACK_FAILED_PACKET_VALIDATION_SIZE = 2,
            ACK_FAILED_OUT_OF_RANGE           = 3,
            ACK_FAILED_SYSTEM_FLASH_FAILURE   = 4,
            ACK_FAILED_SYSTEM_NOT_READY       = 5,
            ACK_FAILED_UNKNOWN_PACKET         = 6
        };

        /** Acknowledgment packet */
        struct Acknowledge
        {
            static constexpr int ID = 0;
            static constexpr int SIZE = 4;

            uint8_t acked_packet_id;
            uint8_t acked_payload_checksum_lsb;
            uint8_t acked_payload_checksum_msb;
            uint8_t result;

            /** Tests whether this acknowledgment matches the given packet
             * header
             */
            bool isMatching(Header const& header) const;

            /** True if this indicates a success */
            bool isSuccess() const;

            /** True if this acknowledge is a failure-to-validate-packet error
             */
            bool isPacketValidationFailure() const;

            /** True if this acknowledge indicates a protocol error on the
             * driver side
             *
             * Packet validation failures are not reported as they might be a
             * communication error as well. Check against sent packets with
             * isMatching to make the difference
             */
            bool isProtocolError() const;

            /** True if this acknowledge indicates a system error on the IMU
             * side
             */
            bool isSystemError() const;

            /** True if this ack indicates that the system is not ready */
            bool isNotReady() const;

            /** Initializes an Acknowledge from raw data
             */
            template<typename RandomInputIterator>
            static Acknowledge unmarshal(RandomInputIterator begin, RandomInputIterator end)
            {
                if (end - begin != sizeof(Acknowledge))
                    throw std::length_error("Acknowledge::unmarshal buffer size not the expected size");
                return Acknowledge{ begin[0], begin[1], begin[2], begin[3] };
            }
        } __attribute__((packed));

        /** Request packet
         *
         * This is a variable-length packet made only of the IDs of the
         * requested packets. There is no static part, hence the empty struct
         */
        struct Request
        {
            static constexpr int ID = 1;
            static constexpr int MIN_SIZE = 0;

            template<typename OutputIterator, typename InputIterator>
            OutputIterator marshal(OutputIterator out, InputIterator begin, InputIterator end) const
            {
                return std::copy(begin, end, out);
            }

            template<typename OutputIterator>
            OutputIterator marshal(OutputIterator out, uint8_t packet_id) const
            {
                return std::copy(&packet_id, &packet_id + 1, out);
            }
        } __attribute__((packed));

        /** Boot modes */
        enum BOOT_MODES
        {
            BOOT_TO_BOOTLOADER,
            BOOT_TO_PROGRAM
        };

        /** Boot mode packet */
        struct BootMode
        {
            static constexpr int ID = 2;
            static constexpr int SIZE = 1;

            uint8_t boot_mode;

            template<typename OutputIterator>
            OutputIterator marshal(OutputIterator out) const
            {
                *out = boot_mode;
                return out + 1;
            }

            template<typename RandomInputIterator>
            static BootMode unmarshal(RandomInputIterator begin, RandomInputIterator end)
            {
                if (end - begin != sizeof(BootMode))
                    throw std::length_error("BootMode::unmarshal: buffer size is not expected size");
                return BootMode{*begin};
            }
        } __attribute__((packed));

        /** Device information */
        struct DeviceInformation
        {
            static constexpr int ID = 3;
            static constexpr int SIZE = 24;

            uint32_t software_version;
            uint32_t device_id;
            uint32_t hardware_revision;
            uint32_t serial_number_part0;
            uint32_t serial_number_part1;
            uint32_t serial_number_part2;

            template<typename RandomInputIterator>
            static DeviceInformation unmarshal(RandomInputIterator begin, RandomInputIterator end)
            {
                if (end - begin != sizeof(DeviceInformation))
                    throw std::length_error("DeviceInformation::unmarshal: buffer size is not expected size");

                DeviceInformation info;
                info.software_version    = read32<uint32_t>(begin);
                info.device_id           = read32<uint32_t>(begin + 4);
                info.hardware_revision   = read32<uint32_t>(begin + 8);
                info.serial_number_part0 = read32<uint32_t>(begin + 12);
                info.serial_number_part1 = read32<uint32_t>(begin + 16);
                info.serial_number_part2 = read32<uint32_t>(begin + 20);
                return info;
            }
        } __attribute__((packed));

        struct RestoreFactorySettings
        {
            static constexpr int ID = 4;
            static constexpr int SIZE = 4;

            uint8_t verification_sequence[4] = { 0x1C, 0x9E, 0x42, 0x85 };

            template<typename OutputIterator>
            OutputIterator marshal(OutputIterator out) const
            {
                std::copy(verification_sequence, verification_sequence + 4, out);
                return out + 4;
            }
        } __attribute__((packed));

        struct HotStartReset
        {
            static constexpr int ID = 5;
            static constexpr int SIZE = 4;

            uint8_t verification_sequence[4] = { 0x7E, 0x7A, 0x05, 0x21 };

            template<typename OutputIterator>
            OutputIterator marshal(OutputIterator out) const
            {
                std::copy(verification_sequence, verification_sequence + 4, out);
                return out + 4;
            }
        } __attribute__((packed));

        struct ColdStartReset
        {
            static constexpr int ID = 5;
            static constexpr int SIZE = 4;

            uint8_t verification_sequence[4] = { 0xB7, 0x38, 0x5D, 0x9A };

            template<typename OutputIterator>
            OutputIterator marshal(OutputIterator out) const
            {
                std::copy(verification_sequence, verification_sequence + 4, out);
                return out + 4;
            }
        } __attribute__((packed));

        enum SYSTEM_STATUS
        {
            SYSTEM_FAILURE                    = 0x0001,
            SYSTEM_ACCELEROMETER_FAILURE      = 0x0002,
            SYSTEM_GYROSCOPE_FAILURE          = 0x0004,
            SYSTEM_MAGNETOMETER_FAILURE       = 0x0008,
            SYSTEM_PRESSURE_SENSOR_FAILURE    = 0x0010,
            SYSTEM_GNSS_SENSOR_FAILURE        = 0x0020,
            SYSTEM_ACCELEROMETER_OVER_RANGE   = 0x0040,
            SYSTEM_GYROSCOPE_OVER_RANGE       = 0x0080,
            SYSTEM_MAGNETOMETER_OVER_RANGE    = 0x0100,
            SYSTEM_PRESSURE_SENSOR_OVER_RANGE = 0x0200,
            SYSTEM_MIN_TEMPERATURE_ALARM      = 0x0400,
            SYSTEM_MAX_TEMPERATURE_ALARM      = 0x0800,
            SYSTEM_LOW_VOLTAGE_ALARM          = 0x1000,
            SYSTEM_HIGH_VOLTAGE_ALARM         = 0x2000,
            SYSTEM_GNSS_ANTENNA_DISCONNECTED  = 0x4000,
            SYSTEM_DATA_OUTPUT_OVERFLOW_ALARM = 0x8000
        };

        enum FILTER_STATUS
        {
            FILTER_ORIENTATION_INITIALIZED      = 0x0001,
            FILTER_NAVIGATION_INITIALIZED       = 0x0002,
            FILTER_HEADING_INITIALIZED          = 0x0004,
            FILTER_UTC_INITIALIZED              = 0x0008,

            FILTER_GNSS_FIX_STATUS_MASK         = 0x0070,
            FILTER_GNSS_NO_FIX                  = 0x0000,
            FILTER_GNSS_2D                      = 0x0010,
            FILTER_GNSS_3D                      = 0x0020,
            FILTER_GNSS_SBAS                    = 0x0030,
            FILTER_GNSS_DGPS                    = 0x0040,
            FILTER_GNSS_OMNISTAR                = 0x0050,
            FILTER_GNSS_RTK_FLOAT               = 0x0060,
            FILTER_GNSS_RTK_FIXED               = 0x0070,

            FILTER_EVENT_1                      = 0x0080,
            FILTER_EVENT_2                      = 0x0100,
            FILTER_INTERNAL_GNSS_ENABLED        = 0x0200,
            FILTER_MAGNETIC_HEADING_ENABLED     = 0x0400,
            FILTER_VELOCITY_HEADING_ENABLED     = 0x0800,
            FILTER_ATMOSPHERIC_ALTITUDE_ENABLED = 0x1000,
            FILTER_EXTERNAL_POSITION_ACTIVE     = 0x2000,
            FILTER_EXTERNAL_VELOCITY_ACTIVE     = 0x4000,
            FILTER_EXTERNAL_HEADING_ACTIVE      = 0x8000,
        };

        struct SystemState
        {
            static constexpr int ID = 20;
            static constexpr int SIZE = 100;

            /** Bitfield of SYSTEM_STATUS */
            uint16_t system_status;
            /** Bitfield of FILTER_STATUS */
            uint16_t filter_status;
            uint32_t unix_time_seconds;
            uint32_t unix_time_microseconds;
            double   lat_lon_z[3];
            float   velocity_ned[3];
            float   body_acceleration_xyz[3];
            float   g;
            float   rpy[3];
            float   angular_velocity[3];
            float   lat_lon_z_stddev[3];

            template<typename InputIterator>
            static SystemState unmarshal(InputIterator begin, InputIterator end)
            {
                if (end - begin != sizeof(SystemState))
                    throw std::length_error("SystemState::unmarshal buffer size not the expected size");

                SystemState state;
                state.system_status            = read16<uint16_t>(begin);
                state.filter_status            = read16<uint16_t>(begin + 2);
                state.unix_time_seconds        = read32<uint32_t>(begin + 4);
                state.unix_time_microseconds   = read32<uint32_t>(begin + 8);

                state.g                        = read32<float>(begin + 60);

                for (int i = 0; i < 3; ++i)
                {
                    state.lat_lon_z[i]             = read64<double>(begin + 12 + 8 * i);
                    state.velocity_ned[i]          = read32<float>(begin + 36 + 4 * i);
                    state.body_acceleration_xyz[i] = read32<float>(begin + 48 + 4 * i);
                    state.rpy[i]                   = read32<float>(begin + 64 + 4 * i);
                    state.angular_velocity[i]      = read32<float>(begin + 76 + 4 * i);
                    state.lat_lon_z_stddev[i]      = read32<float>(begin + 88 + 4 * i);
                }
                return state;
            }
        } __attribute__((packed));

        struct UnixTime
        {
            static constexpr int ID = 21;
            static constexpr int SIZE = 8;

            uint32_t seconds;
            uint32_t microseconds;

            template<typename InputIterator>
            static UnixTime unmarshal(InputIterator begin, InputIterator end)
            {
                if (end - begin != sizeof(UnixTime))
                    throw std::length_error("SystemState::unmarshal buffer size not the expected size");
                return UnixTime{read32<uint32_t>(begin), read32<uint32_t>(begin + 4)};
            }
        } __attribute__((packed));

        struct Status
        {
            static constexpr int ID = 23;
            static constexpr int SIZE = 4;

            /** Bitfield of SYSTEM_STATUS */
            uint16_t system_status;
            /** Bitfield of FILTER_STATUS */
            uint16_t filter_status;

            template<typename InputIterator>
            static Status unmarshal(InputIterator begin, InputIterator end)
            {
                if (end - begin != sizeof(Status))
                    throw std::length_error("SystemState::unmarshal buffer size not the expected size");
                return Status{read16<uint16_t>(begin), read16<uint16_t>(begin + 2)};
            }
        } __attribute__((packed));

        struct GeodeticPositionStandardDeviation
        {
            static constexpr int ID = 24;
            static constexpr int SIZE = 12;

            float   lat_lon_z_stddev[3];

            template<typename InputIterator>
            static GeodeticPositionStandardDeviation unmarshal(InputIterator begin, InputIterator end)
            {
                if (end - begin != sizeof(GeodeticPositionStandardDeviation))
                    throw std::length_error("SystemState::unmarshal buffer size not the expected size");
                return GeodeticPositionStandardDeviation {
                    read32<float>(begin + 0),
                    read32<float>(begin + 4),
                    read32<float>(begin + 8)
                };
            }
        } __attribute__((packed));

        struct NEDVelocityStandardDeviation
        {
            static constexpr int ID = 25;
            static constexpr int SIZE = 12;

            float ned[3];

            template<typename InputIterator>
            static NEDVelocityStandardDeviation unmarshal(InputIterator begin, InputIterator end)
            {
                if (end - begin != sizeof(NEDVelocityStandardDeviation))
                    throw std::length_error("NEDVelocityStandardDeviation::unmarshal buffer size not the expected size");
                return NEDVelocityStandardDeviation {
                    read32<float>(begin + 0),
                    read32<float>(begin + 4),
                    read32<float>(begin + 8)
                };
            }
        } __attribute__((packed));

        struct EulerOrientationStandardDeviation
        {
            static constexpr int ID = 26;
            static constexpr int SIZE = 12;

            float rpy[3];

            template<typename InputIterator>
            static EulerOrientationStandardDeviation unmarshal(InputIterator begin, InputIterator end)
            {
                if (end - begin != sizeof(EulerOrientationStandardDeviation))
                    throw std::length_error("EulerOrientationStandardDeviation::unmarshal buffer size not the expected size");
                return EulerOrientationStandardDeviation {
                    read32<float>(begin + 0),
                    read32<float>(begin + 4),
                    read32<float>(begin + 8)
                };
            }
        } __attribute__((packed));

        struct RawSensors
        {
            static constexpr int ID = 28;
            static constexpr int SIZE = 48;

            float accelerometers_xyz[3];
            float gyroscope_xyz[3];
            float magnetometer_xyz[3];
            float imu_temperature_C;
            float pressure;
            float pressure_temperature_C;

            template<typename InputIterator>
            static RawSensors unmarshal(InputIterator begin, InputIterator end)
            {
                if (end - begin != sizeof(RawSensors))
                    throw std::length_error("RawSensors::unmarshal buffer size not the expected size");

                RawSensors out;
                for (int i = 0; i < 3; ++i)
                {
                    out.accelerometers_xyz[i] = read32<float>(begin + 0 + 4 * i);
                    out.gyroscope_xyz[i]      = read32<float>(begin + 12 + 4 * i);
                    out.magnetometer_xyz[i]   = read32<float>(begin + 24 + 4 * i);
                }

                out.imu_temperature_C = read32<float>(begin + 36);
                out.pressure = read32<float>(begin + 40);
                out.pressure_temperature_C = read32<float>(begin + 44);
                return out;
            }
        } __attribute__((packed));

        struct RawGNSS
        {
            static constexpr int ID   = 29;
            static constexpr int SIZE = 74;

            uint32_t unix_time_seconds;
            uint32_t unix_time_microseconds;
            double   lat_lon_z[3];
            float    velocity_ned[3];
            float    lat_lon_z_stddev[3];
            float    pitch;
            float    yaw;
            float    pitch_stddev;
            float    yaw_stddev;
            /** Bitfield described by RAW_GNSS_STATUS */
            uint16_t status;

            template<typename InputIterator>
            static RawGNSS unmarshal(InputIterator begin, InputIterator end)
            {
                if (end - begin != SIZE)
                    throw std::length_error("RawGNSS::unmarshal buffer size not the expected size");

                RawGNSS out;
                out.unix_time_seconds = read32<uint32_t>(begin + 0);
                out.unix_time_microseconds = read32<uint32_t>(begin + 4);
                for (int i = 0; i < 3; ++i)
                {
                    out.lat_lon_z[i] = read64<double>(begin + 8 + 8 * i);
                    out.velocity_ned[i] = read32<float>(begin + 32 + 4 * i);
                    out.lat_lon_z_stddev[i] = read32<float>(begin + 44 + 4 * i);
                }

                out.pitch = read32<float>(begin + 56);
                out.yaw = read32<float>(begin + 60);
                out.pitch_stddev = read32<float>(begin + 64);
                out.yaw_stddev = read32<float>(begin + 68);
                out.status = read16<uint16_t>(begin + 72);
                return out;
            }
        } __attribute__((packed));

        enum RAW_GNSS_STATUS
        {
            RAW_GNSS_FIX_STATUS_MASK                = 0x07,
            RAW_GNSS_NO_FIX                         = 0x00,
            RAW_GNSS_2D                             = 0x01,
            RAW_GNSS_3D                             = 0x02,
            RAW_GNSS_SBAS                           = 0x03,
            RAW_GNSS_DGPS                           = 0x04,
            RAW_GNSS_OMNISTAR                       = 0x05,
            RAW_GNSS_RTK_FLOAT                      = 0x06,
            RAW_GNSS_RTK_FIXED                      = 0x07,

            RAW_GNSS_HAS_DOPPLER_VELOCITY           = 0x08,
            RAW_GNSS_HAS_TIME                       = 0x10,
            RAW_GNSS_EXTERNAL                       = 0x20,
            RAW_GNSS_HAS_TILT                       = 0x40,
            RAW_GNSS_HAS_HEADING                    = 0x80,
            RAW_GNSS_HAS_FLOATING_AMBIGUITY_HEADING = 0x100
        };

        struct Satellites
        {
            static constexpr int ID   = 30;
            static constexpr int SIZE = 13;

            float hdop;
            float vdop;
            uint8_t gps_satellite_count;
            uint8_t glonass_satellite_count;
            uint8_t beidou_satellite_count;
            uint8_t galileo_satellite_count;
            uint8_t sbas_satellite_count;

            template<typename InputIterator>
            static Satellites unmarshal(InputIterator begin, InputIterator end)
            {
                if (end - begin != SIZE)
                    throw std::length_error("Satellites::unmarshal buffer size not the expected size");

                return Satellites {
                    read32<float>(begin + 0),
                    read32<float>(begin + 4),
                    begin[8],
                    begin[9],
                    begin[10],
                    begin[11],
                    begin[12]
                };
            }
        } __attribute__((packed));

        enum SATELLITE_SYSTEM
        {
            SATELLITE_SYSTEM_UNKNOWN,
            SATELLITE_SYSTEM_GPS,
            SATELLITE_SYSTEM_GLONASS,
            SATELLITE_SYSTEM_BEIDOU,
            SATELLITE_SYSTEM_GALILEO,
            SATELLITE_SYSTEM_SBAS,
            SATELLITE_SYSTEM_QZSS,
            SATELLITE_SYSTEM_STARFIRE,
            SATELLITE_SYSTEM_OMNISTAR
        };

        /** Bitfield values to represent supported frequencies */
        enum SATELLITE_FREQUENCIES
        {
            SATELLITE_FREQUENCY_L1CA = 0x01,
            SATELLITE_FREQUENCY_L1C  = 0x02,
            SATELLITE_FREQUENCY_L1P  = 0x04,
            SATELLITE_FREQUENCY_L1M  = 0x08,
            SATELLITE_FREQUENCY_L2C  = 0x10,
            SATELLITE_FREQUENCY_L2P  = 0x20,
            SATELLITE_FREQUENCY_L2M  = 0x40,
            SATELLITE_FREQUENCY_L5   = 0x80
        };

        /** Satellite info as returned by DetailedSatellites */
        struct SatelliteInfo
        {
            static constexpr int SIZE = 7;

            /** The satellite system as represented by SATELLITE_SYSTEM */
            uint8_t system;
            /** The satellite ID number */
            uint8_t prn;
            /** Satellite frequencies
             *
             * Bitfield represented by the SATELLITE_FREQUENCIES enum
             */
            uint8_t frequencies;
            /** Elevation in degrees */
            uint8_t elevation;
            /** Azimuth in degrees */
            uint16_t azimuth;
            /** Signal to noise ratio in dB */
            uint8_t snr;

            template<typename InputIterator>
            static SatelliteInfo unmarshal(InputIterator begin, InputIterator end)
            {
                return SatelliteInfo{
                    begin[0], begin[1], begin[2], begin[3],
                    read16<uint16_t>(begin + 4), begin[6]
                };
            }
        } __attribute__((packed));

        struct DetailedSatellites
        {
            static constexpr int ID = 31;
            static constexpr int MIN_SIZE = 0;

            template<typename InputIterator>
            static void unmarshal(InputIterator begin, InputIterator end, std::vector<SatelliteInfo>& info)
            {
                if ((end - begin) % SatelliteInfo::SIZE != 0)
                    throw std::length_error("Satellites::unmarshal buffer is not a multiple of the SatelliteInfo size");

                for (; begin != end; begin += SatelliteInfo::SIZE)
                {
                    info.push_back(SatelliteInfo::unmarshal(begin, begin + SatelliteInfo::SIZE));
                }
            }
        } __attribute__((packed));

        struct NEDVelocity
        {
            static constexpr int ID = 35;
            static constexpr int SIZE = 12;

            float ned[3];

            template<typename InputIterator>
            static NEDVelocity unmarshal(InputIterator begin, InputIterator end)
            {
                if ((end - begin) != SIZE)
                    throw std::length_error("NEDVelocity::unmarshal buffer is not of the expected size");

                return NEDVelocity { {
                    read32<float>(begin),
                    read32<float>(begin + 4),
                    read32<float>(begin + 8) } };
            }
        } __attribute__((packed));

        struct BodyVelocity
        {
            static constexpr int ID = 36;
            static constexpr int SIZE = 12;

            float xyz[3];

            template<typename InputIterator>
            static BodyVelocity unmarshal(InputIterator begin, InputIterator end)
            {
                if ((end - begin) != SIZE)
                    throw std::length_error("BodyVelocity::unmarshal buffer is not of the expected size");

                return BodyVelocity { {
                    read32<float>(begin),
                    read32<float>(begin + 4),
                    read32<float>(begin + 8) } };
            }
        } __attribute__((packed));

        /** Acceleration with the G force removed */
        struct Acceleration
        {
            static constexpr int ID = 37;
            static constexpr int SIZE = 12;

            float xyz[3];

            template<typename InputIterator>
            static Acceleration unmarshal(InputIterator begin, InputIterator end)
            {
                if ((end - begin) != SIZE)
                    throw std::length_error("Acceleration::unmarshal buffer is not of the expected size");

                return Acceleration { {
                    read32<float>(begin),
                    read32<float>(begin + 4),
                    read32<float>(begin + 8) } };
            }
        } __attribute__((packed));

        struct BodyAcceleration
        {
            static constexpr int ID = 38;
            static constexpr int SIZE = 16;

            float xyz[3];
            float g;

            template<typename InputIterator>
            static BodyAcceleration unmarshal(InputIterator begin, InputIterator end)
            {
                if ((end - begin) != SIZE)
                    throw std::length_error("BodyAcceleration::unmarshal buffer is not of the expected size");

                return BodyAcceleration {
                    {
                        read32<float>(begin),
                        read32<float>(begin + 4),
                        read32<float>(begin + 8)
                    },
                    read32<float>(begin + 12)
                };
            }
        } __attribute__((packed));

        struct QuaternionOrientation
        {
            static constexpr int ID = 40;
            static constexpr int SIZE = 16;

            float im;
            float xyz[3];

            template<typename InputIterator>
            static QuaternionOrientation unmarshal(InputIterator begin, InputIterator end)
            {
                if ((end - begin) != SIZE)
                    throw std::length_error("QuaternionOrientation::unmarshal buffer is not of the expected size");

                return QuaternionOrientation {
                    read32<float>(begin + 0),
                    {
                        read32<float>(begin + 4),
                        read32<float>(begin + 8),
                        read32<float>(begin + 12)
                    }
                };
            }
        } __attribute__((packed));

        struct AngularVelocity
        {
            static constexpr int ID = 42;
            static constexpr int SIZE = 12;

            float xyz[3];

            template<typename InputIterator>
            static AngularVelocity unmarshal(InputIterator begin, InputIterator end)
            {
                if ((end - begin) != SIZE)
                    throw std::length_error("AngularVelocity::unmarshal buffer is not of the expected size");

                return AngularVelocity { {
                    read32<float>(begin),
                    read32<float>(begin + 4),
                    read32<float>(begin + 8) } };
            }
        } __attribute__((packed));

        struct AngularAcceleration
        {
            static constexpr int ID = 43;
            static constexpr int SIZE = 12;

            float xyz[3];

            template<typename InputIterator>
            static AngularAcceleration unmarshal(InputIterator begin, InputIterator end)
            {
                if ((end - begin) != SIZE)
                    throw std::length_error("AngularAcceleration::unmarshal buffer is not of the expected size");

                return AngularAcceleration { {
                    read32<float>(begin),
                    read32<float>(begin + 4),
                    read32<float>(begin + 8) } };
            }
        } __attribute__((packed));

        struct LocalMagneticField
        {
            static constexpr int ID = 50;
            static constexpr int SIZE = 12;

            float xyz[3];

            template<typename InputIterator>
            static LocalMagneticField unmarshal(InputIterator begin, InputIterator end)
            {
                if ((end - begin) != SIZE)
                    throw std::length_error("LocalMagneticField::unmarshal buffer is not of the expected size");

                return LocalMagneticField { {
                    read32<float>(begin),
                    read32<float>(begin + 4),
                    read32<float>(begin + 8) } };
            }
        } __attribute__((packed));

        struct PacketTimerPeriod
        {
            static constexpr int ID = 180;
            static constexpr int SIZE = 4;

            uint8_t  permanent;
            uint8_t  utc_synchronization;
            uint16_t period;

            template<typename OutputIterator>
            OutputIterator marshal(OutputIterator out) const
            {
                out[0] = permanent;
                out[1] = utc_synchronization;
                write16(out + 2, period);
                return out + SIZE;
            }

            template<typename InputIterator>
            static PacketTimerPeriod unmarshal(InputIterator begin, InputIterator end)
            {
                if (end - begin != SIZE)
                    throw std::length_error("PacketTimerPeriod::unmarshal buffer size not the expected size");

                PacketTimerPeriod out;
                out.permanent = 0;
                out.utc_synchronization = *(begin + 1);
                out.period = read16<uint16_t>(begin + 2);
                return out;
            }
        } __attribute__((packed));

        struct PacketPeriods
        {
            static constexpr int ID = 181;
            static constexpr int MIN_SIZE = 2;
            static constexpr int PERIOD_SIZE = 5;

            typedef std::map<uint8_t, uint32_t> Periods;

            uint8_t permanent;
            uint8_t clear_existing;

            template<typename OutputIterator, typename InputIterator>
            OutputIterator marshal(OutputIterator out, InputIterator begin, InputIterator end) const
            {
                out[0] = permanent;
                out[1] = clear_existing;
                for (out += 2; begin != end; ++begin, out += PERIOD_SIZE)
                {
                    out[0] = begin->first;
                    write32(out + 1, begin->second);
                }
                return out;
            }

            template<typename InputIterator>
            static std::map<uint8_t, uint32_t> unmarshal(InputIterator begin, InputIterator end)
            {
                if ((end - begin) < MIN_SIZE)
                    throw std::length_error("PacketPeriods::unmarshal buffer too small");
                else if ((end - begin - MIN_SIZE) % PERIOD_SIZE != 0)
                    throw std::length_error("PacketPeriods::unmarshal expected period list to be a multiple of 5");
                begin += MIN_SIZE;
                std::map<uint8_t, uint32_t> result;
                for (; begin != end; begin += PERIOD_SIZE)
                    result[*begin] = read32<uint32_t>(begin + 1);
                return result;
            }
        } __attribute__((packed));

        struct BaudRates
        {
            static constexpr int ID = 182;
            static constexpr int SIZE = 17;

            uint8_t permanent;
            uint32_t primary_port;
            uint32_t gpio;
            uint32_t auxiliary_rs232;
            uint32_t reserved;

            template<typename OutputIterator>
            OutputIterator marshal(OutputIterator out) const
            {
                out[0] = permanent;
                write32(out + 1, primary_port);
                write32(out + 5, gpio);
                write32(out + 9, auxiliary_rs232);
                write32(out + 13, static_cast<uint32_t>(0));
                return out + 17;
            }

            template<typename InputIterator>
            static BaudRates unmarshal(InputIterator begin, InputIterator end)
            {
                if (end - begin != SIZE)
                    throw std::length_error("BaudRates::unmarshal buffer size not the expected size");

                return BaudRates {
                    0,
                    read32<uint32_t>(begin + 1),
                    read32<uint32_t>(begin + 5),
                    read32<uint32_t>(begin + 9),
                    static_cast<uint32_t>(0)
                };
            }
        } __attribute__((packed));

        struct Alignment
        {
            static constexpr int ID = 185;
            static constexpr int SIZE = 73;

            uint8_t permanent;
            float dcm[9];
            float gnss_antenna_offset_xyz[3];
            float odometer_offset_xyz[3];
            float external_data_offset_xyz[3];

            template<typename OutputIterator>
            OutputIterator marshal(OutputIterator out) const
            {
                out[0] = permanent;
                for (int i = 0; i < 9; ++i)
                    write32(out + 1 + 4 * i, dcm[i]);
                for (int i = 0; i < 3; ++i)
                {
                    write32(out + 37 + 4 * i, gnss_antenna_offset_xyz[i]);
                    write32(out + 49 + 4 * i, odometer_offset_xyz[i]);
                    write32(out + 61 + 4 * i, external_data_offset_xyz[i]);
                }
                return out + SIZE;
            }

            template<typename InputIterator>
            static Alignment unmarshal(InputIterator begin, InputIterator end)
            {
                if (end - begin != SIZE)
                    throw std::length_error("Alignment::unmarshal buffer size not the expected size");

                Alignment out;
                out.permanent = 0;
                for (int i = 0; i < 9; ++i)
                    out.dcm[i] = read32<float>(begin + 1 + 4 * i);
                for (int i = 0; i < 3; ++i)
                {
                    out.gnss_antenna_offset_xyz[i]  = read32<float>(begin + 37 + 4 * i);
                    out.odometer_offset_xyz[i]      = read32<float>(begin + 49 + 4 * i);
                    out.external_data_offset_xyz[i] = read32<float>(begin + 61 + 4 * i);
                }
                return out;
            }
        } __attribute__((packed));

        struct FilterOptions
        {
            static constexpr int ID   = 186;
            static constexpr int SIZE = 17;

            uint8_t permanent;
            /** The vehicle type to tune the filter dynamics
             *
             * See VEHICLE_TYPES
             */
            uint8_t vehicle_type;
            uint8_t enabled_internal_gnss;
            uint8_t reserved_0 = 0;
            uint8_t enabled_atmospheric_altitude;
            uint8_t enabled_velocity_heading;
            uint8_t enabled_reversing_detection;
            uint8_t enabled_motion_analysis;
            uint8_t reserved_1[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };

            template<typename OutputIterator>
            OutputIterator marshal(OutputIterator out) const
            {
                return std::copy(&permanent, reserved_1 + 9, out);
            }

            template<typename InputIterator>
            static FilterOptions unmarshal(InputIterator begin, InputIterator end)
            {
                if (end - begin != SIZE)
                    throw std::length_error("MagneticCalibrationValues::unmarshal buffer size not the expected size");

                FilterOptions out;
                out.permanent = 0;
                std::copy(begin + 1, end, &out.vehicle_type);
                return out;
            }
        } __attribute__ ((packed));

        enum VEHICLE_TYPES
        {
            VEHICLE_UNCONSTRAINED         = 0,
            VEHICLE_BICYCLE_OR_MOTORCYCLE = 1,
            VEHICLE_CAR                   = 2,
            VEHICLE_HOVERCRAFT            = 3,
            VEHICLE_SUBMARINE             = 4,
            VEHICLE_3D_UNDERWATER         = 5,
            VEHICLE_FIXED_WING_PLANE      = 6,
            VEHICLE_3D_AIRCRAFT           = 7,
            VEHICLE_HUMAN                 = 8,
            VEHICLE_BOAT                  = 9,
            VEHICLE_LARGE_SHIP            = 10,
            VEHICLE_STATIONARY            = 11,
            VEHICLE_STUNT_PLANE           = 12,
            VEHICLE_RACE_CAR              = 13
        };

        struct MagneticCalibrationValues
        {
            static constexpr int ID   = 189;
            static constexpr int SIZE = 49;

            uint8_t permanent;
            float hard_iron_bias_xyz[3];
            float soft_iron_transformation[9];

            template<typename OutputIterator>
            OutputIterator marshal(OutputIterator out) const
            {
                out[0] = permanent;
                for (int i = 0; i < 3; ++i)
                    write32(out + 1 + 4 * i, hard_iron_bias_xyz[i]);
                for (int i = 0; i < 9; ++i)
                    write32(out + 13 + 4 * i, soft_iron_transformation[i]);
                return out + SIZE;
            }

            template<typename InputIterator>
            static MagneticCalibrationValues unmarshal(InputIterator begin, InputIterator end)
            {
                if (end - begin != SIZE)
                    throw std::length_error("MagneticCalibrationValues::unmarshal buffer size not the expected size");

                MagneticCalibrationValues out;
                out.permanent = 0;
                for (int i = 0; i < 3; ++i)
                    out.hard_iron_bias_xyz[i] = read32<float>(begin + 1 + 4 * i);
                for (int i = 0; i < 9; ++i)
                    out.soft_iron_transformation[i] = read32<float>(begin + 13 + 4 * i);
                return out;
            }
        } __attribute__((packed));

        struct MagneticCalibrationConfiguration
        {
            static constexpr int ID = 190;
            static constexpr int SIZE = 1;

            /** Action as one of MAGNETIC_CALIBRATION_ACTIONS */
            uint8_t action;

            template<typename OutputIterator>
            OutputIterator marshal(OutputIterator out) const
            {
                out[0] = action;
                return out + SIZE;
            }
        } __attribute__((packed));

        enum MAGNETIC_CALIBRATION_ACTIONS
        {
            MAGNETIC_CALIBRATION_CANCEL,
            MAGNETIC_CALIBRATION_START_2D,
            MAGNETIC_CALIBRATION_START_3D,
            MAGNETIC_CALIBRATION_RESET
        };

        struct MagneticCalibrationStatus
        {
            static constexpr int ID = 191;
            static constexpr int SIZE = 3;

            /** Status as one of MAGNETIC_CALIBRATION_STATUS */
            uint8_t status;
            uint8_t progress;
            uint8_t error;

            template<typename InputIterator>
            static MagneticCalibrationStatus unmarshal(InputIterator begin, InputIterator end)
            {
                if (end - begin != SIZE)
                    throw std::length_error("MagneticCalibrationStatus::unmarshal buffer size not the expected size");

                MagneticCalibrationStatus out;
                std::copy(begin, end, &out.status);
                return out;
            }
        } __attribute__((packed));

        enum MAGNETIC_CALIBRATION_STATUS
        {
            MAGNETIC_CALIBRATION_NOT_COMPLETED,
            MAGNETIC_CALIBRATION_2D_COMPLETED,
            MAGNETIC_CALIBRATION_3D_COMPLETED,
            MAGNETIC_CALIBRATION_CUSTOM_COMPLETED,
            MAGNETIC_CALIBRATION_2D_IN_PROGRESS,
            MAGNETIC_CALIBRATION_3D_IN_PROGRESS,
            MAGNETIC_CALIBRATION_ERROR_2D_EXCESSIVE_ROLL,
            MAGNETIC_CALIBRATION_ERROR_2D_EXCESSIVE_PITCH,
            MAGNETIC_CALIBRATION_ERROR_SENSOR_OVER_RANGE,
            MAGNETIC_CALIBRATION_ERROR_SENSOR_TIME_OUT,
            MAGNETIC_CALIBRATION_ERROR_SENSOR_SYSTEM_ERROR,
            MAGNETIC_CALIBRATION_ERROR_SENSOR_INTERFERENCE_ERROR
        };
    }
}

#endif
