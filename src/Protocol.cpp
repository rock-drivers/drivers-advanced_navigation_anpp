#include <imu_advanced_navigation_anpp/Protocol.hpp>
#include <boost/crc.hpp>
#include <cstddef>
#include <stdexcept>

using namespace std;
using namespace imu_advanced_navigation_anpp;
using namespace imu_advanced_navigation_anpp::protocol;

uint8_t const protocol::Acknowledge::ID;
uint8_t const protocol::Request::ID;
uint8_t const protocol::BootMode::ID;
uint8_t const protocol::DeviceInformation::ID;
uint8_t const protocol::RestoreFactorySettings::ID;
uint8_t const protocol::HotStartReset::ID;
uint8_t const protocol::ColdStartReset::ID;
uint8_t const protocol::SystemState::ID;
uint8_t const protocol::UnixTime::ID;
uint8_t const protocol::Status::ID;
uint8_t const protocol::GeodeticPositionStandardDeviation::ID;
uint8_t const protocol::NEDVelocityStandardDeviation::ID;
uint8_t const protocol::EulerOrientationStandardDeviation::ID;
uint8_t const protocol::RawSensors::ID;
uint8_t const protocol::RawGNSS::ID;
uint8_t const protocol::Satellites::ID;
uint8_t const protocol::DetailedSatellites::ID;
uint8_t const protocol::NEDVelocity::ID;
uint8_t const protocol::BodyVelocity::ID;
uint8_t const protocol::Acceleration::ID;
uint8_t const protocol::BodyAcceleration::ID;
uint8_t const protocol::QuaternionOrientation::ID;
uint8_t const protocol::AngularVelocity::ID;
uint8_t const protocol::AngularAcceleration::ID;
uint8_t const protocol::LocalMagneticField::ID;
uint8_t const protocol::PacketTimerPeriod::ID;
uint8_t const protocol::PacketPeriods::ID;
uint8_t const protocol::BaudRates::ID;
uint8_t const protocol::Alignment::ID;
uint8_t const protocol::FilterOptions::ID;
uint8_t const protocol::MagneticCalibrationValues::ID;
uint8_t const protocol::MagneticCalibrationConfiguration::ID;
uint8_t const protocol::MagneticCalibrationStatus::ID;

Header::Header()
    : header_checksum(1)
    , packet_id(0)
    , payload_length(0)
    , payload_checksum_lsb(0)
    , payload_checksum_msb(0)
{
}

Header::Header(uint8_t packet_id, uint8_t const* begin, uint8_t const* end)
    : packet_id(packet_id)
    , payload_length(end - begin)
{
    uint16_t checksum = protocol::crc(begin, end);
    payload_checksum_lsb = (checksum & 0xFF);
    payload_checksum_msb = (checksum >> 8) & 0xFF;
    header_checksum = computeHeaderChecksum();
}

uint16_t protocol::crc(uint8_t const* begin, uint8_t const* end)
{
    boost::crc_ccitt_type crc;
    crc.process_bytes(begin, end - begin);
    return crc.checksum();
}

size_t Header::getPacketLength() const
{
    return payload_length + SIZE;
}

uint8_t Header::computeHeaderChecksum() const
{
    return ((packet_id + payload_length + payload_checksum_lsb + payload_checksum_msb) ^ 0xFF) + 1;
}

bool Header::isValid() const
{
    return header_checksum == computeHeaderChecksum();
}

bool Header::isPacketValid(uint8_t const* begin, uint8_t const* end) const
{
    if (static_cast<ptrdiff_t>(payload_length) != (end - begin))
        return false;
    uint16_t checksum = protocol::crc(begin, end);
    return (payload_checksum_lsb == (checksum & 0xFF)) && (payload_checksum_msb == ((checksum >> 8) & 0xFF));
}

bool Acknowledge::isMatching(Header const& header) const
{
    return acked_packet_id == header.packet_id &&
        acked_payload_checksum_lsb == header.payload_checksum_lsb &&
        acked_payload_checksum_msb == header.payload_checksum_msb;
}

bool Acknowledge::isSuccess() const
{
    return result == ACK_SUCCESS;
}

bool Acknowledge::isPacketValidationFailure() const
{
    return result == ACK_FAILED_PACKET_VALIDATION_CRC ||
        result == ACK_FAILED_PACKET_VALIDATION_SIZE;
}

bool Acknowledge::isProtocolError() const
{
    return result == ACK_FAILED_OUT_OF_RANGE ||
        result == ACK_FAILED_UNKNOWN_PACKET;
}

bool Acknowledge::isSystemError() const
{
    return result == ACK_FAILED_SYSTEM_FLASH_FAILURE;
}

bool Acknowledge::isNotReady() const
{
    return result == ACK_FAILED_SYSTEM_NOT_READY;
}

