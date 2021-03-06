#include "test_Helpers.hpp"
#include <imu_advanced_navigation_anpp/Protocol.hpp>
#include <imu_advanced_navigation_anpp/Driver.hpp>
#include <list>
#include <stdexcept>
#include <cstring>

using namespace std;
using namespace imu_advanced_navigation_anpp::protocol;
using namespace testing;

static_assert(sizeof(Header) == Header::SIZE, "SIZE and sizeof() do not agree");

TEST(protocol_Header, it_is_invalid_when_constructed)
{
    Header header;
    ASSERT_FALSE(header.isValid());
    uint8_t packet[0];
    ASSERT_FALSE(header.isPacketValid(packet, packet));
}

static const int PAYLOAD_SIZE = 7;
static const uint8_t payload[7] = { '0', '1', '2', '3', '4', '5', '6' };

TEST(protocol_Header, it_initializes_a_valid_packet_from_data)
{
    Header header(5, payload, payload + PAYLOAD_SIZE);
    ASSERT_TRUE(header.isValid());
    ASSERT_TRUE(header.isPacketValid(payload, payload + PAYLOAD_SIZE));
    ASSERT_EQ(5, header.packet_id);
    ASSERT_EQ(PAYLOAD_SIZE, header.payload_length);
}

TEST(protocol_Header, isValid_returns_true_if_the_header_checksum_matches)
{
    Header header;
    header.header_checksum = 0;
    header.packet_id = 0;
    header.payload_length = 0;
    header.payload_checksum_lsb = 0; // calculated online
    header.payload_checksum_msb = 0;
    ASSERT_TRUE(header.isValid());
}

TEST(protocol_Header, isPacketValid_returns_true_if_the_packet_size_and_checksum_match)
{
    Header header;
    header.payload_length = PAYLOAD_SIZE;
    header.payload_checksum_lsb = 0xA7; // calculated online
    header.payload_checksum_msb = 0x88;
    ASSERT_TRUE(header.isPacketValid(payload, payload + 7));
}

TEST(protocol_Header, isPacketValid_returns_false_if_the_checksum_matches_but_the_header_size_is_higher)
{
    Header header;
    header.payload_length = PAYLOAD_SIZE + 1;
    header.payload_checksum_lsb = 0xA7; // calculated online
    header.payload_checksum_msb = 0x88;
    ASSERT_FALSE(header.isPacketValid(payload, payload + PAYLOAD_SIZE));
}

TEST(protocol_Header, isPacketValid_returns_false_if_the_checksum_matches_but_the_header_size_is_smaller)
{
    Header header;
    header.payload_length = PAYLOAD_SIZE - 1;
    header.payload_checksum_lsb = 0xA7; // calculated online
    header.payload_checksum_msb = 0x88;
    ASSERT_FALSE(header.isPacketValid(payload, payload + PAYLOAD_SIZE));
}

TEST(protocol_Header, isPacketValid_returns_false_if_the_length_matches_but_not_the_checksum)
{
    Header header;
    uint8_t payload[7] = { '0', '1', '2', '3', '4', '5', '7' };
    header.payload_length = PAYLOAD_SIZE;
    header.payload_checksum_lsb = 0xA7; // calculated online
    header.payload_checksum_msb = 0x88;
    ASSERT_FALSE(header.isPacketValid(payload, payload + PAYLOAD_SIZE));
}

static_assert(sizeof(Acknowledge) == Acknowledge::SIZE, "SIZE and sizeof() do not agree");

TEST(protocol_Acknowledge, isMatching_returns_true_if_the_id_and_checksum_match)
{
    Header header(1, nullptr, nullptr);
    Acknowledge ack = { 1, header.payload_checksum_lsb, header.payload_checksum_msb, imu_advanced_navigation_anpp::ACK_SUCCESS };
    ASSERT_TRUE(ack.isMatching(header));
}

TEST(protocol_Acknowledge, isMatching_returns_false_if_the_id_does_not_match)
{
    Header header(1, nullptr, nullptr);
    Acknowledge ack = { 2, header.payload_checksum_lsb, header.payload_checksum_msb, imu_advanced_navigation_anpp::ACK_SUCCESS };
    ASSERT_FALSE(ack.isMatching(header));
}

TEST(protocol_Acknowledge, isMatching_returns_false_if_the_checksum_lsb_does_not_match)
{
    Header header(1, nullptr, nullptr);
    Acknowledge ack = { 1, 0x10, header.payload_checksum_msb, imu_advanced_navigation_anpp::ACK_SUCCESS };
    ASSERT_FALSE(ack.isMatching(header));
}

TEST(protocol_Acknowledge, isMatching_returns_false_if_the_checksum_msb_does_not_match)
{
    Header header(1, nullptr, nullptr);
    Acknowledge ack = { 1, header.payload_checksum_lsb, 0x10, imu_advanced_navigation_anpp::ACK_SUCCESS };
    ASSERT_FALSE(ack.isMatching(header));
}


template<typename Predicate>
void validateAcknowledgePredicate(std::list<imu_advanced_navigation_anpp::ACK_RESULTS> expected_true, Predicate predicate)
{
    for (uint8_t i = 0; i < 8; ++i)
    {
        Acknowledge ack = { 1, 0, 0, i };
        if (expected_true.front() == i)
        {
            ASSERT_TRUE(predicate(ack));
            expected_true.pop_front();
        }
        else
        {
            ASSERT_FALSE(predicate(ack));
        }

    }
}

TEST(protocol_Acknowledge, isSuccess)
{
    validateAcknowledgePredicate({ imu_advanced_navigation_anpp::ACK_SUCCESS },
            [](Acknowledge ack){ return ack.isSuccess(); });
}

TEST(protocol_Acknowledge, isPacketValidationFailure)
{
    validateAcknowledgePredicate({ imu_advanced_navigation_anpp::ACK_FAILED_PACKET_VALIDATION_CRC, imu_advanced_navigation_anpp::ACK_FAILED_PACKET_VALIDATION_SIZE },
            [](Acknowledge ack){ return ack.isPacketValidationFailure(); });
}

TEST(protocol_Acknowledge, isProtocolError)
{
    validateAcknowledgePredicate({ imu_advanced_navigation_anpp::ACK_FAILED_OUT_OF_RANGE, imu_advanced_navigation_anpp::ACK_FAILED_UNKNOWN_PACKET },
            [](Acknowledge ack){ return ack.isProtocolError(); });
}

TEST(protocol_Acknowledge, isNotReady)
{
    validateAcknowledgePredicate({ imu_advanced_navigation_anpp::ACK_FAILED_SYSTEM_NOT_READY },
            [](Acknowledge ack){ return ack.isNotReady(); });
}

TEST(protocol_Acknowledge, isSystemError)
{
    validateAcknowledgePredicate({ imu_advanced_navigation_anpp::ACK_FAILED_SYSTEM_FLASH_FAILURE },
            [](Acknowledge ack){ return ack.isSystemError(); });
}

TEST(protocol_Acknowledge, unmarshal)
{
    uint8_t data[5] = { 1, 2, 3, imu_advanced_navigation_anpp::ACK_SUCCESS };
    Acknowledge ack = Acknowledge::unmarshal(data, data + 4);
    ASSERT_EQ(1, ack.acked_packet_id);
    ASSERT_EQ(2, ack.acked_payload_checksum_lsb);
    ASSERT_EQ(3, ack.acked_payload_checksum_msb);
    ASSERT_EQ(imu_advanced_navigation_anpp::ACK_SUCCESS, ack.result);
}

TEST(protocol_Acknowledge, unmarshal_fails_if_too_little_data_is_provided_and_does_not_access_any_of_it)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(Acknowledge::unmarshal(ptr, ptr + 3), std::length_error);
}

TEST(protocol_Acknowledge, unmarshal_fails_if_too_much_data_is_provided_and_does_not_access_any_of_it)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(Acknowledge::unmarshal(ptr, ptr + 5), std::length_error);
}

TEST(protocol_Request, marshal_copies_the_packet_ids_to_the_output)
{
    vector<uint8_t> out{ 0, 0, 0 };
    uint8_t const packet_ids[3] = { PacketTimerPeriod::ID, BodyVelocity::ID, NEDVelocity::ID };
    auto out_end = Request().marshal(out.begin(), packet_ids, packet_ids + 3);
    ASSERT_EQ(out_end, out.end());
    ASSERT_THAT(out, ElementsAre(PacketTimerPeriod::ID, BodyVelocity::ID, NEDVelocity::ID));
}

static_assert(sizeof(BootMode) == BootMode::SIZE, "SIZE and sizeof() do not agree");

TEST(protocol_BootMode, marshal)
{
    vector<uint8_t> out{ 0xFF };
    auto out_end = BootMode{ BOOT_TO_BOOTLOADER }.marshal(out.begin());
    ASSERT_EQ(out_end, out.end());
    ASSERT_THAT(out, ElementsAre(BOOT_TO_BOOTLOADER));
}

TEST(protocol_BootMode, unmarshal)
{
    vector<uint8_t> out{ BOOT_TO_PROGRAM };
    BootMode mode = BootMode::unmarshal(out.begin(), out.end());
    ASSERT_EQ(BOOT_TO_PROGRAM, mode.boot_mode);
}

TEST(protocol_BootMode, unmarshal_fails_if_too_little_data_is_provided_and_does_not_access_any_of_it)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(BootMode::unmarshal(ptr, ptr), std::length_error);
}

TEST(protocol_BootMode, unmarshal_fails_if_too_much_data_is_provided_and_does_not_access_any_of_it)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(BootMode::unmarshal(ptr, ptr + 2), std::length_error);
}

static_assert(sizeof(DeviceInformation) == DeviceInformation::SIZE, "SIZE and sizeof() do not agree");

TEST(protocol_DeviceInformation, unmarshal)
{
    uint8_t device_info[24];
    std::iota(device_info, device_info + 24, 0);
    DeviceInformation info = DeviceInformation::unmarshal(device_info, device_info + 24);
    ASSERT_EQ(0x03020100, info.software_version);
    ASSERT_EQ(0x07060504, info.device_id);
    ASSERT_EQ(0x0b0a0908, info.hardware_revision);
    ASSERT_EQ(0x0f0e0d0c, info.serial_number_part0);
    ASSERT_EQ(0x13121110, info.serial_number_part1);
    ASSERT_EQ(0x17161514, info.serial_number_part2);
}

TEST(protocol_DeviceInformation, unmarshal_fails_if_too_little_data_is_provided_and_does_not_access_any_of_it)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(DeviceInformation::unmarshal(ptr, ptr + 23), std::length_error);
}

TEST(protocol_DeviceInformation, unmarshal_fails_if_too_much_data_is_provided_and_does_not_access_any_of_it)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(DeviceInformation::unmarshal(ptr, ptr + 25), std::length_error);
}

static_assert(sizeof(RestoreFactorySettings) == RestoreFactorySettings::SIZE, "SIZE and sizeof() do not agree");
static_assert(sizeof(HotStartReset) == HotStartReset::SIZE, "SIZE and sizeof() do not agree");
static_assert(sizeof(ColdStartReset) == ColdStartReset::SIZE, "SIZE and sizeof() do not agree");

static_assert(sizeof(SystemState) == SystemState::SIZE, "SIZE and sizeof() do not agree");

TEST(protocol_SystemState, unmarshal)
{
    uint8_t marshalled[100];
    SystemState expected;
    expected.system_status = 0x0201;
    RAW_SET(marshalled,      { 0x01, 0x02 } );
    expected.filter_status = 0x0403;
    RAW_SET(marshalled + 2,  { 0x03, 0x04 } );
    expected.unix_time_seconds = 0x08070605;
    RAW_SET(marshalled + 4,  { 0x05, 0x06, 0x07, 0x08 } );
    expected.unix_time_microseconds = 0x0c0b0a09;
    RAW_SET(marshalled + 8,  { 0x09, 0x0a, 0x0b, 0x0c } );

    // Floating-point values below have been generated by taking a  seed value
    // (the first one, starting with the 123456... sequence) and then adding one
    // to the first and last byte, checking the corresponding FP value
    //
    // Found FP-to-binary converters online
    expected.lat_lon_z[0] = TEST_FP8[0].fp;
    RAW_SET(marshalled + 12, TEST_FP8[0].binary );
    expected.lat_lon_z[1] = TEST_FP8[1].fp;
    RAW_SET(marshalled + 20, TEST_FP8[1].binary );
    expected.lat_lon_z[2] = TEST_FP8[2].fp;
    RAW_SET(marshalled + 28, TEST_FP8[2].binary );
    expected.velocity_ned[0] = TEST_FP4[0].fp;
    RAW_SET(marshalled + 36, TEST_FP4[0].binary );
    expected.velocity_ned[1] = TEST_FP4[1].fp;
    RAW_SET(marshalled + 40, TEST_FP4[1].binary );
    expected.velocity_ned[2] = TEST_FP4[2].fp;
    RAW_SET(marshalled + 44, TEST_FP4[2].binary );
    expected.body_acceleration_xyz[0] = TEST_FP4[3].fp;
    RAW_SET(marshalled + 48, TEST_FP4[3].binary );
    expected.body_acceleration_xyz[1] = TEST_FP4[4].fp;
    RAW_SET(marshalled + 52, TEST_FP4[4].binary );
    expected.body_acceleration_xyz[2] = TEST_FP4[5].fp;
    RAW_SET(marshalled + 56, TEST_FP4[5].binary );
    expected.g = TEST_FP4[6].fp;
    RAW_SET(marshalled + 60, TEST_FP4[6].binary );
    expected.rpy[0] = TEST_FP4[7].fp;
    RAW_SET(marshalled + 64, TEST_FP4[7].binary );
    expected.rpy[1] = TEST_FP4[8].fp;
    RAW_SET(marshalled + 68, TEST_FP4[8].binary );
    expected.rpy[2] = TEST_FP4[9].fp;
    RAW_SET(marshalled + 72, TEST_FP4[9].binary );
    expected.angular_velocity[0] = TEST_FP4[10].fp;
    RAW_SET(marshalled + 76, TEST_FP4[10].binary );
    expected.angular_velocity[1] = TEST_FP4[11].fp;
    RAW_SET(marshalled + 80, TEST_FP4[11].binary );
    expected.angular_velocity[2] = TEST_FP4[12].fp;
    RAW_SET(marshalled + 84, TEST_FP4[12].binary );
    expected.lat_lon_z_stddev[0] = TEST_FP4[13].fp;
    RAW_SET(marshalled + 88, TEST_FP4[13].binary );
    expected.lat_lon_z_stddev[1] = TEST_FP4[14].fp;
    RAW_SET(marshalled + 92, TEST_FP4[14].binary );
    expected.lat_lon_z_stddev[2] = TEST_FP4[15].fp;
    RAW_SET(marshalled + 96, TEST_FP4[15].binary );

    SystemState unmarshalled = SystemState::unmarshal(marshalled, marshalled + 100);
    ASSERT_FALSE(std::memcmp(&expected, &unmarshalled, sizeof(SystemState)));
}

TEST(protocol_SystemState, unmarshal_throws_if_given_too_little_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(SystemState::unmarshal(ptr, ptr + 99), std::length_error);
}

TEST(protocol_SystemState, unmarshal_throws_if_given_too_much_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(SystemState::unmarshal(ptr, ptr + 101), std::length_error);
}

static_assert(sizeof(UnixTime) == UnixTime::SIZE, "SIZE and sizeof() do not agree");

TEST(protocol_UnixTime, unmarshal)
{
    uint8_t marshalled[100];
    UnixTime expected;
    expected.seconds = 0x08070605;
    RAW_SET(marshalled,  { 0x05, 0x06, 0x07, 0x08 } );
    expected.microseconds = 0x0c0b0a09;
    RAW_SET(marshalled + 4,  { 0x09, 0x0a, 0x0b, 0x0c } );
    UnixTime unmarshalled = UnixTime::unmarshal(marshalled, marshalled + 8);
    ASSERT_FALSE(std::memcmp(&expected, &unmarshalled, sizeof(UnixTime)));
}

TEST(protocol_UnixTime, unmarshal_throws_if_given_too_little_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(UnixTime::unmarshal(ptr, ptr + 7), std::length_error);
}

TEST(protocol_UnixTime, unmarshal_throws_if_given_too_much_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(UnixTime::unmarshal(ptr, ptr + 9), std::length_error);
}

static_assert(sizeof(Status) == Status::SIZE, "SIZE and sizeof() do not agree");

TEST(protocol_Status, unmarshal)
{
    uint8_t marshalled[100];
    Status expected;
    expected.system_status = 0x0201;
    RAW_SET(marshalled,      { 0x01, 0x02 } );
    expected.filter_status = 0x0403;
    RAW_SET(marshalled + 2,  { 0x03, 0x04 } );
    Status unmarshalled = Status::unmarshal(marshalled, marshalled + 4);
    ASSERT_FALSE(std::memcmp(&expected, &unmarshalled, sizeof(Status)));
}

TEST(protocol_Status, unmarshal_throws_if_given_too_little_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(Status::unmarshal(ptr, ptr + 3), std::length_error);
}

TEST(protocol_Status, unmarshal_throws_if_given_too_much_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(Status::unmarshal(ptr, ptr + 5), std::length_error);
}

static_assert(sizeof(GeodeticPositionStandardDeviation) == GeodeticPositionStandardDeviation::SIZE, "SIZE and sizeof() do not agree");

TEST(protocol_GeodeticPositionStandardDeviation, unmarshal)
{
    uint8_t marshalled[100];
    GeodeticPositionStandardDeviation expected;
    expected.lat_lon_z_stddev[0] = TEST_FP4[13].fp;
    RAW_SET(marshalled + 0, TEST_FP4[13].binary );
    expected.lat_lon_z_stddev[1] = TEST_FP4[14].fp;
    RAW_SET(marshalled + 4, TEST_FP4[14].binary );
    expected.lat_lon_z_stddev[2] = TEST_FP4[15].fp;
    RAW_SET(marshalled + 8, TEST_FP4[15].binary );
    GeodeticPositionStandardDeviation unmarshalled =
        GeodeticPositionStandardDeviation::unmarshal(marshalled, marshalled + 12);
    ASSERT_FALSE(std::memcmp(&expected, &unmarshalled, sizeof(GeodeticPositionStandardDeviation)));
}

TEST(protocol_GeodeticPositionStandardDeviation, unmarshal_throws_if_given_too_little_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(GeodeticPositionStandardDeviation::unmarshal(ptr, ptr + 11), std::length_error);
}

TEST(protocol_GeodeticPositionStandardDeviation, unmarshal_throws_if_given_too_much_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(GeodeticPositionStandardDeviation::unmarshal(ptr, ptr + 13), std::length_error);
}

static_assert(sizeof(NEDVelocityStandardDeviation) == NEDVelocityStandardDeviation::SIZE, "SIZE and sizeof() do not agree");

TEST(protocol_NEDVelocityStandardDeviation, unmarshal)
{
    uint8_t marshalled[100];
    NEDVelocityStandardDeviation expected;
    expected.ned[0] = TEST_FP4[0].fp;
    RAW_SET(marshalled + 0, TEST_FP4[0].binary );
    expected.ned[1] = TEST_FP4[1].fp;
    RAW_SET(marshalled + 4, TEST_FP4[1].binary );
    expected.ned[2] = TEST_FP4[2].fp;
    RAW_SET(marshalled + 8, TEST_FP4[2].binary );
    NEDVelocityStandardDeviation unmarshalled =
        NEDVelocityStandardDeviation::unmarshal(marshalled, marshalled + 12);
    ASSERT_FALSE(std::memcmp(&expected, &unmarshalled, sizeof(NEDVelocityStandardDeviation)));
}

TEST(protocol_NEDVelocityStandardDeviation, unmarshal_throws_if_given_too_little_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(NEDVelocityStandardDeviation::unmarshal(ptr, ptr + 11), std::length_error);
}

TEST(protocol_NEDVelocityStandardDeviation, unmarshal_throws_if_given_too_much_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(NEDVelocityStandardDeviation::unmarshal(ptr, ptr + 13), std::length_error);
}

static_assert(sizeof(EulerOrientationStandardDeviation) == EulerOrientationStandardDeviation::SIZE, "SIZE and sizeof() do not agree");

TEST(protocol_EulerOrientationStandardDeviation, unmarshal)
{
    uint8_t marshalled[100];
    EulerOrientationStandardDeviation expected;
    expected.rpy[0] = TEST_FP4[0].fp;
    RAW_SET(marshalled + 0, TEST_FP4[0].binary );
    expected.rpy[1] = TEST_FP4[1].fp;
    RAW_SET(marshalled + 4, TEST_FP4[1].binary );
    expected.rpy[2] = TEST_FP4[2].fp;
    RAW_SET(marshalled + 8, TEST_FP4[2].binary );
    EulerOrientationStandardDeviation unmarshalled =
        EulerOrientationStandardDeviation::unmarshal(marshalled, marshalled + 12);
    ASSERT_FALSE(std::memcmp(&expected, &unmarshalled, sizeof(EulerOrientationStandardDeviation)));
}

TEST(protocol_EulerOrientationStandardDeviation, unmarshal_throws_if_given_too_little_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(EulerOrientationStandardDeviation::unmarshal(ptr, ptr + 11), std::length_error);
}

TEST(protocol_EulerOrientationStandardDeviation, unmarshal_throws_if_given_too_much_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(EulerOrientationStandardDeviation::unmarshal(ptr, ptr + 13), std::length_error);
}

static_assert(sizeof(RawSensors) == RawSensors::SIZE, "SIZE and sizeof() do not agree");

TEST(protocol_RawSensors, unmarshal)
{
    uint8_t marshalled[100];
    RawSensors expected;
    expected.accelerometers_xyz[0] = TEST_FP4[0].fp;
    RAW_SET(marshalled + 0, TEST_FP4[0].binary );
    expected.accelerometers_xyz[1] = TEST_FP4[1].fp;
    RAW_SET(marshalled + 4, TEST_FP4[1].binary );
    expected.accelerometers_xyz[2] = TEST_FP4[2].fp;
    RAW_SET(marshalled + 8, TEST_FP4[2].binary );
    expected.gyroscopes_xyz[0] = TEST_FP4[3].fp;
    RAW_SET(marshalled + 12, TEST_FP4[3].binary );
    expected.gyroscopes_xyz[1] = TEST_FP4[4].fp;
    RAW_SET(marshalled + 16, TEST_FP4[4].binary );
    expected.gyroscopes_xyz[2] = TEST_FP4[5].fp;
    RAW_SET(marshalled + 20, TEST_FP4[5].binary );
    expected.magnetometers_xyz[0] = TEST_FP4[6].fp;
    RAW_SET(marshalled + 24, TEST_FP4[6].binary );
    expected.magnetometers_xyz[1] = TEST_FP4[7].fp;
    RAW_SET(marshalled + 28, TEST_FP4[7].binary );
    expected.magnetometers_xyz[2] = TEST_FP4[8].fp;
    RAW_SET(marshalled + 32, TEST_FP4[8].binary );
    expected.imu_temperature_C = TEST_FP4[9].fp;
    RAW_SET(marshalled + 36, TEST_FP4[9].binary );
    expected.pressure = TEST_FP4[10].fp;
    RAW_SET(marshalled + 40, TEST_FP4[10].binary );
    expected.pressure_temperature_C = TEST_FP4[11].fp;
    RAW_SET(marshalled + 44, TEST_FP4[11].binary );

    RawSensors unmarshalled =
        RawSensors::unmarshal(marshalled, marshalled + 48);
    ASSERT_FALSE(std::memcmp(&expected, &unmarshalled, sizeof(RawSensors)));
}

TEST(protocol_RawSensors, unmarshal_throws_if_given_too_little_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(RawSensors::unmarshal(ptr, ptr + 47), std::length_error);
}

TEST(protocol_RawSensors, unmarshal_throws_if_given_too_much_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(RawSensors::unmarshal(ptr, ptr + 49), std::length_error);
}

static_assert(sizeof(RawGNSS) == RawGNSS::SIZE, "SIZE and sizeof() do not agree");

TEST(protocol_RawGNSS, unmarshal)
{
    uint8_t marshalled[RawGNSS::SIZE];
    RawGNSS expected;
    expected.unix_time_seconds = 0x08070605;
    expected.unix_time_microseconds = 0x0c0b0a09;
    expected.lat_lon_z[0]           = TEST_FP8[0].fp;
    expected.lat_lon_z[1]           = TEST_FP8[1].fp;
    expected.lat_lon_z[2]           = TEST_FP8[2].fp;
    expected.velocity_ned[0]        = TEST_FP4[0].fp;
    expected.velocity_ned[1]        = TEST_FP4[1].fp;
    expected.velocity_ned[2]        = TEST_FP4[2].fp;
    expected.lat_lon_z_stddev[0]    = TEST_FP4[3].fp;
    expected.lat_lon_z_stddev[1]    = TEST_FP4[4].fp;
    expected.lat_lon_z_stddev[2]    = TEST_FP4[5].fp;
    expected.pitch                  = TEST_FP4[6].fp;
    expected.yaw                    = TEST_FP4[7].fp;
    expected.pitch_stddev           = TEST_FP4[8].fp;
    expected.yaw_stddev             = TEST_FP4[9].fp;
    expected.status                 = 0x0201;

    RAW_SET(marshalled + 0,  { 0x05, 0x06, 0x07, 0x08 } );
    RAW_SET(marshalled + 4,  { 0x09, 0x0a, 0x0b, 0x0c } );
    RAW_SET(marshalled + 8,  TEST_FP8[0].binary);
    RAW_SET(marshalled + 16, TEST_FP8[1].binary);
    RAW_SET(marshalled + 24, TEST_FP8[2].binary);
    RAW_SET(marshalled + 32, TEST_FP4[0].binary);
    RAW_SET(marshalled + 36, TEST_FP4[1].binary);
    RAW_SET(marshalled + 40, TEST_FP4[2].binary);
    RAW_SET(marshalled + 44, TEST_FP4[3].binary);
    RAW_SET(marshalled + 48, TEST_FP4[4].binary);
    RAW_SET(marshalled + 52, TEST_FP4[5].binary);
    RAW_SET(marshalled + 56, TEST_FP4[6].binary);
    RAW_SET(marshalled + 60, TEST_FP4[7].binary);
    RAW_SET(marshalled + 64, TEST_FP4[8].binary);
    RAW_SET(marshalled + 68, TEST_FP4[9].binary);
    RAW_SET(marshalled + 72, { 0x01, 0x02 });

    RawGNSS unmarshalled =
        RawGNSS::unmarshal(marshalled, marshalled + RawGNSS::SIZE);
    ASSERT_FALSE(std::memcmp(&expected, &unmarshalled, sizeof(RawGNSS)));
}

TEST(protocol_RawGNSS, unmarshal_throws_if_given_too_little_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(RawGNSS::unmarshal(ptr, ptr + RawGNSS::SIZE - 1), std::length_error);
}

TEST(protocol_RawGNSS, unmarshal_throws_if_given_too_much_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(RawGNSS::unmarshal(ptr, ptr + RawGNSS::SIZE + 1), std::length_error);
}

static_assert(sizeof(Satellites) == Satellites::SIZE, "SIZE and sizeof() do not agree");

TEST(protocol_Satellites, unmarshal)
{
    uint8_t marshalled[Satellites::SIZE];
    Satellites expected;
    expected.hdop = TEST_FP4[0].fp;
    expected.vdop = TEST_FP4[1].fp;
    expected.gps_satellite_count     = 0;
    expected.glonass_satellite_count = 1;
    expected.beidou_satellite_count  = 2;
    expected.galileo_satellite_count = 3;
    expected.sbas_satellite_count    = 4;

    RAW_SET(marshalled + 0,  TEST_FP4[0].binary );
    RAW_SET(marshalled + 4,  TEST_FP4[1].binary );
    marshalled[8] = 0;
    marshalled[9] = 1;
    marshalled[10] = 2;
    marshalled[11] = 3;
    marshalled[12] = 4;

    Satellites unmarshalled =
        Satellites::unmarshal(marshalled, marshalled + Satellites::SIZE);
    ASSERT_FALSE(std::memcmp(&expected, &unmarshalled, sizeof(Satellites)));
}

TEST(protocol_Satellites, unmarshal_throws_if_given_too_little_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(Satellites::unmarshal(ptr, ptr + Satellites::SIZE - 1), std::length_error);
}

TEST(protocol_Satellites, unmarshal_throws_if_given_too_much_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(Satellites::unmarshal(ptr, ptr + Satellites::SIZE + 1), std::length_error);
}

static_assert(sizeof(SatelliteInfo) == SatelliteInfo::SIZE, "SIZE and sizeof() do not agree");

TEST(protocol_DetailedSatellites, unmarshal)
{
    uint8_t marshalled[2 * SatelliteInfo::SIZE];
    std::vector<SatelliteInfo> expected {
        { 0x1, 0x2, 0x3, 0x4, 0x0102, 0x5 },
        { 0x6, 0x7, 0x8, 0x9, 0x0304, 0xa }
    };

    marshalled[0] = 0x1;
    marshalled[1] = 0x2;
    marshalled[2] = 0x3;
    marshalled[3] = 0x4;
    RAW_SET(marshalled + 4,  { 0x02, 0x01 } );
    marshalled[6] = 0x5;

    marshalled[7] = 0x6;
    marshalled[8] = 0x7;
    marshalled[9] = 0x8;
    marshalled[10] = 0x9;
    RAW_SET(marshalled + 11,  { 0x04, 0x03 } );
    marshalled[13] = 0xa;

    std::vector<SatelliteInfo> unmarshalled;
    DetailedSatellites::unmarshal(marshalled, marshalled + 2 * SatelliteInfo::SIZE, unmarshalled);
    ASSERT_EQ(2, unmarshalled.size());
    ASSERT_FALSE(std::memcmp(expected.data(), unmarshalled.data(), 2 * SatelliteInfo::SIZE));
}

TEST(protocol_DetailedSatellites, unmarshals_an_empty_array)
{
    uint8_t* ptr = nullptr;
    std::vector<SatelliteInfo> unmarshalled;
    DetailedSatellites::unmarshal(ptr, ptr, unmarshalled);
    ASSERT_TRUE(unmarshalled.empty());
}

TEST(protocol_DetailedSatellites, unmarshal_throws_if_given_an_amount_of_data_that_is_not_a_multiple_of_7)
{
    uint8_t* ptr = nullptr;
    std::vector<SatelliteInfo> unmarshalled;
    ASSERT_THROW(DetailedSatellites::unmarshal(ptr, ptr + 2 * SatelliteInfo::SIZE - 1, unmarshalled), std::length_error);
}

TEST(protocol_DetailedSatellites, unmarshal_throws_if_given_too_much_data)
{
    uint8_t* ptr = nullptr;
    std::vector<SatelliteInfo> unmarshalled;
    ASSERT_THROW(DetailedSatellites::unmarshal(ptr, ptr + 2 * SatelliteInfo::SIZE + 1, unmarshalled), std::length_error);
}

static_assert(sizeof(NEDVelocity) == NEDVelocity::SIZE, "SIZE and sizeof() do not agree");

TEST(protocol_NEDVelocity, unmarshal)
{
    uint8_t marshalled[NEDVelocity::SIZE];
    NEDVelocity expected;
    expected.ned[0] = TEST_FP4[0].fp;
    expected.ned[1] = TEST_FP4[1].fp;
    expected.ned[2] = TEST_FP4[2].fp;

    RAW_SET(marshalled + 0,  TEST_FP4[0].binary );
    RAW_SET(marshalled + 4,  TEST_FP4[1].binary );
    RAW_SET(marshalled + 8,  TEST_FP4[2].binary );

    NEDVelocity unmarshalled =
        NEDVelocity::unmarshal(marshalled, marshalled + NEDVelocity::SIZE);
    ASSERT_FALSE(std::memcmp(&expected, &unmarshalled, sizeof(NEDVelocity)));
}

TEST(protocol_NEDVelocity, unmarshal_throws_if_given_too_little_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(NEDVelocity::unmarshal(ptr, ptr + NEDVelocity::SIZE - 1), std::length_error);
}

TEST(protocol_NEDVelocity, unmarshal_throws_if_given_too_much_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(NEDVelocity::unmarshal(ptr, ptr + NEDVelocity::SIZE + 1), std::length_error);
}

static_assert(sizeof(GeodeticPosition) == GeodeticPosition::SIZE, "SIZE and sizeof() do not agree");

TEST(protocol_GeodeticPosition, unmarshal)
{
    GeodeticPosition expected;
    expected.lat_lon_z[0] = TEST_FP8[0].fp;
    expected.lat_lon_z[1] = TEST_FP8[1].fp;
    expected.lat_lon_z[2] = TEST_FP8[2].fp;

    uint8_t marshalled[GeodeticPosition::SIZE];
    RAW_SET(marshalled + 0, TEST_FP8[0].binary );
    RAW_SET(marshalled + 8, TEST_FP8[1].binary );
    RAW_SET(marshalled + 16, TEST_FP8[2].binary );

    GeodeticPosition unmarshalled =
        GeodeticPosition::unmarshal(marshalled, marshalled + GeodeticPosition::SIZE);
    ASSERT_FALSE(std::memcmp(&expected, &unmarshalled, sizeof(GeodeticPosition)));
}

TEST(protocol_GeodeticPosition, unmarshal_throws_if_given_too_little_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(GeodeticPosition::unmarshal(ptr, ptr + 11), std::length_error);
}

TEST(protocol_GeodeticPosition, unmarshal_throws_if_given_too_much_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(GeodeticPosition::unmarshal(ptr, ptr + 13), std::length_error);
}

static_assert(sizeof(BodyVelocity) == BodyVelocity::SIZE, "SIZE and sizeof() do not agree");

TEST(protocol_BodyVelocity, unmarshal)
{
    uint8_t marshalled[BodyVelocity::SIZE];
    BodyVelocity expected;
    expected.xyz[0] = TEST_FP4[0].fp;
    expected.xyz[1] = TEST_FP4[1].fp;
    expected.xyz[2] = TEST_FP4[2].fp;

    RAW_SET(marshalled + 0,  TEST_FP4[0].binary );
    RAW_SET(marshalled + 4,  TEST_FP4[1].binary );
    RAW_SET(marshalled + 8,  TEST_FP4[2].binary );

    BodyVelocity unmarshalled =
        BodyVelocity::unmarshal(marshalled, marshalled + BodyVelocity::SIZE);
    ASSERT_FALSE(std::memcmp(&expected, &unmarshalled, sizeof(BodyVelocity)));
}

TEST(protocol_BodyVelocity, unmarshal_throws_if_given_too_little_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(BodyVelocity::unmarshal(ptr, ptr + BodyVelocity::SIZE - 1), std::length_error);
}

TEST(protocol_BodyVelocity, unmarshal_throws_if_given_too_much_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(BodyVelocity::unmarshal(ptr, ptr + BodyVelocity::SIZE + 1), std::length_error);
}

static_assert(sizeof(Acceleration) == Acceleration::SIZE, "SIZE and sizeof() do not agree");

TEST(protocol_Acceleration, unmarshal)
{
    uint8_t marshalled[Acceleration::SIZE];
    Acceleration expected;
    expected.xyz[0] = TEST_FP4[0].fp;
    expected.xyz[1] = TEST_FP4[1].fp;
    expected.xyz[2] = TEST_FP4[2].fp;

    RAW_SET(marshalled + 0,  TEST_FP4[0].binary );
    RAW_SET(marshalled + 4,  TEST_FP4[1].binary );
    RAW_SET(marshalled + 8,  TEST_FP4[2].binary );

    Acceleration unmarshalled =
        Acceleration::unmarshal(marshalled, marshalled + Acceleration::SIZE);
    ASSERT_FALSE(std::memcmp(&expected, &unmarshalled, sizeof(Acceleration)));
}

TEST(protocol_Acceleration, unmarshal_throws_if_given_too_little_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(Acceleration::unmarshal(ptr, ptr + Acceleration::SIZE - 1), std::length_error);
}

TEST(protocol_Acceleration, unmarshal_throws_if_given_too_much_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(Acceleration::unmarshal(ptr, ptr + Acceleration::SIZE + 1), std::length_error);
}

static_assert(sizeof(BodyAcceleration) == BodyAcceleration::SIZE, "SIZE and sizeof() do not agree");

TEST(protocol_BodyAcceleration, unmarshal)
{
    uint8_t marshalled[BodyAcceleration::SIZE];
    BodyAcceleration expected;
    expected.xyz[0] = TEST_FP4[0].fp;
    expected.xyz[1] = TEST_FP4[1].fp;
    expected.xyz[2] = TEST_FP4[2].fp;
    expected.g      = TEST_FP4[3].fp;

    RAW_SET(marshalled + 0,  TEST_FP4[0].binary );
    RAW_SET(marshalled + 4,  TEST_FP4[1].binary );
    RAW_SET(marshalled + 8,  TEST_FP4[2].binary );
    RAW_SET(marshalled + 12,  TEST_FP4[3].binary );

    BodyAcceleration unmarshalled =
        BodyAcceleration::unmarshal(marshalled, marshalled + BodyAcceleration::SIZE);
    ASSERT_FALSE(std::memcmp(&expected, &unmarshalled, sizeof(BodyAcceleration)));
}

TEST(protocol_BodyAcceleration, unmarshal_throws_if_given_too_little_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(BodyAcceleration::unmarshal(ptr, ptr + BodyAcceleration::SIZE - 1), std::length_error);
}

TEST(protocol_BodyAcceleration, unmarshal_throws_if_given_too_much_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(BodyAcceleration::unmarshal(ptr, ptr + BodyAcceleration::SIZE + 1), std::length_error);
}

static_assert(sizeof(QuaternionOrientation) == QuaternionOrientation::SIZE, "SIZE and sizeof() do not agree");

TEST(protocol_QuaternionOrientation, unmarshal)
{
    uint8_t marshalled[QuaternionOrientation::SIZE];
    QuaternionOrientation expected;
    expected.im     = TEST_FP4[0].fp;
    expected.xyz[0] = TEST_FP4[1].fp;
    expected.xyz[1] = TEST_FP4[2].fp;
    expected.xyz[2] = TEST_FP4[3].fp;

    RAW_SET(marshalled + 0,  TEST_FP4[0].binary );
    RAW_SET(marshalled + 4,  TEST_FP4[1].binary );
    RAW_SET(marshalled + 8,  TEST_FP4[2].binary );
    RAW_SET(marshalled + 12,  TEST_FP4[3].binary );

    QuaternionOrientation unmarshalled =
        QuaternionOrientation::unmarshal(marshalled, marshalled + QuaternionOrientation::SIZE);
    ASSERT_FALSE(std::memcmp(&expected, &unmarshalled, sizeof(QuaternionOrientation)));
}

TEST(protocol_QuaternionOrientation, unmarshal_throws_if_given_too_little_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(QuaternionOrientation::unmarshal(ptr, ptr + QuaternionOrientation::SIZE - 1), std::length_error);
}

TEST(protocol_QuaternionOrientation, unmarshal_throws_if_given_too_much_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(QuaternionOrientation::unmarshal(ptr, ptr + QuaternionOrientation::SIZE + 1), std::length_error);
}

static_assert(sizeof(AngularVelocity) == AngularVelocity::SIZE, "SIZE and sizeof() do not agree");

TEST(protocol_AngularVelocity, unmarshal)
{
    uint8_t marshalled[AngularVelocity::SIZE];
    AngularVelocity expected;
    expected.xyz[0] = TEST_FP4[0].fp;
    expected.xyz[1] = TEST_FP4[1].fp;
    expected.xyz[2] = TEST_FP4[2].fp;

    RAW_SET(marshalled + 0,  TEST_FP4[0].binary );
    RAW_SET(marshalled + 4,  TEST_FP4[1].binary );
    RAW_SET(marshalled + 8,  TEST_FP4[2].binary );

    AngularVelocity unmarshalled =
        AngularVelocity::unmarshal(marshalled, marshalled + AngularVelocity::SIZE);
    ASSERT_FALSE(std::memcmp(&expected, &unmarshalled, sizeof(AngularVelocity)));
}

TEST(protocol_AngularVelocity, unmarshal_throws_if_given_too_little_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(AngularVelocity::unmarshal(ptr, ptr + AngularVelocity::SIZE - 1), std::length_error);
}

TEST(protocol_AngularVelocity, unmarshal_throws_if_given_too_much_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(AngularVelocity::unmarshal(ptr, ptr + AngularVelocity::SIZE + 1), std::length_error);
}


static_assert(sizeof(AngularAcceleration) == AngularAcceleration::SIZE, "SIZE and sizeof() do not agree");

TEST(protocol_AngularAcceleration, unmarshal)
{
    uint8_t marshalled[AngularAcceleration::SIZE];
    AngularAcceleration expected;
    expected.xyz[0] = TEST_FP4[0].fp;
    expected.xyz[1] = TEST_FP4[1].fp;
    expected.xyz[2] = TEST_FP4[2].fp;

    RAW_SET(marshalled + 0,  TEST_FP4[0].binary );
    RAW_SET(marshalled + 4,  TEST_FP4[1].binary );
    RAW_SET(marshalled + 8,  TEST_FP4[2].binary );

    AngularAcceleration unmarshalled =
        AngularAcceleration::unmarshal(marshalled, marshalled + AngularAcceleration::SIZE);
    ASSERT_FALSE(std::memcmp(&expected, &unmarshalled, sizeof(AngularAcceleration)));
}

TEST(protocol_AngularAcceleration, unmarshal_throws_if_given_too_little_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(AngularAcceleration::unmarshal(ptr, ptr + AngularAcceleration::SIZE - 1), std::length_error);
}

TEST(protocol_AngularAcceleration, unmarshal_throws_if_given_too_much_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(AngularAcceleration::unmarshal(ptr, ptr + AngularAcceleration::SIZE + 1), std::length_error);
}

static_assert(sizeof(LocalMagneticField) == LocalMagneticField::SIZE, "SIZE and sizeof() do not agree");

TEST(protocol_LocalMagneticField, unmarshal)
{
    uint8_t marshalled[LocalMagneticField::SIZE];
    LocalMagneticField expected;
    expected.xyz[0] = TEST_FP4[0].fp;
    expected.xyz[1] = TEST_FP4[1].fp;
    expected.xyz[2] = TEST_FP4[2].fp;

    RAW_SET(marshalled + 0,  TEST_FP4[0].binary );
    RAW_SET(marshalled + 4,  TEST_FP4[1].binary );
    RAW_SET(marshalled + 8,  TEST_FP4[2].binary );

    LocalMagneticField unmarshalled =
        LocalMagneticField::unmarshal(marshalled, marshalled + LocalMagneticField::SIZE);
    ASSERT_FALSE(std::memcmp(&expected, &unmarshalled, sizeof(LocalMagneticField)));
}

TEST(protocol_LocalMagneticField, unmarshal_throws_if_given_too_little_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(LocalMagneticField::unmarshal(ptr, ptr + LocalMagneticField::SIZE - 1), std::length_error);
}

TEST(protocol_LocalMagneticField, unmarshal_throws_if_given_too_much_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(LocalMagneticField::unmarshal(ptr, ptr + LocalMagneticField::SIZE + 1), std::length_error);
}

static_assert(sizeof(NorthSeekingInitializationStatus) == NorthSeekingInitializationStatus::SIZE, "SIZE and sizeof() do not agree");

TEST(protocol_NorthSeekingInitializationStatus, unmarshal)
{
    NorthSeekingInitializationStatus expected;
    expected.flags = 0x0201;
    for (int i = 0; i < 4; ++i)
        expected.progress[i] = i + 3;
    expected.current_rotation_angle         = TEST_FP4[0].fp;
    expected.gyroscope_bias_solution_xyz[0] = TEST_FP4[1].fp;
    expected.gyroscope_bias_solution_xyz[1] = TEST_FP4[2].fp;
    expected.gyroscope_bias_solution_xyz[2] = TEST_FP4[3].fp;
    expected.gyroscope_bias_solution_error  = TEST_FP4[4].fp;

    uint8_t marshalled[NorthSeekingInitializationStatus::SIZE];
    RAW_SET(marshalled + 0,  { 1, 2, 0, 0, 3, 4, 5, 6 });
    for (int i = 0; i < 5; ++i)
        RAW_SET(marshalled + 8 + 4 * i,  TEST_FP4[i].binary );

    NorthSeekingInitializationStatus unmarshalled =
        NorthSeekingInitializationStatus::unmarshal(marshalled, marshalled + NorthSeekingInitializationStatus::SIZE);
    ASSERT_FALSE(std::memcmp(&expected, &unmarshalled, sizeof(NorthSeekingInitializationStatus)));
}

TEST(protocol_NorthSeekingInitializationStatus, unmarshal_throws_if_given_too_little_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(NorthSeekingInitializationStatus::unmarshal(ptr, ptr + NorthSeekingInitializationStatus::SIZE - 1), std::length_error);
}

TEST(protocol_NorthSeekingInitializationStatus, unmarshal_throws_if_given_too_much_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(NorthSeekingInitializationStatus::unmarshal(ptr, ptr + NorthSeekingInitializationStatus::SIZE + 1), std::length_error);
}


static_assert(sizeof(PacketTimerPeriod) == PacketTimerPeriod::SIZE, "SIZE and sizeof() do not agree");

TEST(protocol_PacketTimerPeriod, marshal)
{
    uint8_t marshalled[PacketTimerPeriod::SIZE];
    uint8_t expected[] = { 0x1, 0x2, 0x3, 0x4 };

    PacketTimerPeriod data = { 1, 2, 0x0403 };
    auto out = data.marshal(marshalled);
    ASSERT_EQ(out, marshalled + PacketTimerPeriod::SIZE);
    ASSERT_FALSE(std::memcmp(expected, marshalled, sizeof(marshalled)));
}

TEST(protocol_PacketTimerPeriod, unmarshal)
{
    uint8_t marshalled[PacketTimerPeriod::SIZE] = { 0x1, 0x2, 0x3, 0x4 };
    PacketTimerPeriod expected = { 0, 2, 0x0403 };

    auto unmarshalled = PacketTimerPeriod::unmarshal(marshalled, marshalled + PacketTimerPeriod::SIZE);
    ASSERT_FALSE(std::memcmp(&expected, &unmarshalled, PacketTimerPeriod::SIZE));
}

TEST(protocol_PacketTimerPeriod, unmarshal_throws_if_too_little_data_is_given)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(PacketTimerPeriod::unmarshal(ptr, ptr + PacketTimerPeriod::SIZE - 1), std::length_error);
}

TEST(protocol_PacketTimerPeriod, unmarshal_throws_if_too_much_data_is_given)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(PacketTimerPeriod::unmarshal(ptr, ptr + PacketTimerPeriod::SIZE + 1), std::length_error);
}

static_assert(sizeof(PacketPeriods) == PacketPeriods::MIN_SIZE, "MIN_SIZE and sizeof() do not agree");

TEST(protocol_PacketPeriods, marshal)
{
    uint8_t marshalled[PacketPeriods::MIN_SIZE + PacketPeriods::PERIOD_SIZE * 2];
    uint8_t expected[] = {
        0x1, 0x1,
        0x1, 0x1, 0x2, 0x3, 0x4,
        0x2, 0x5, 0x6, 0x7, 0x8
    };

    PacketPeriods data;
    data.permanent = 1;
    data.clear_existing = 1;
    PacketPeriods::Periods periods {
        { 1, 0x04030201 },
        { 2, 0x08070605 }
    };

    auto out = data.marshal(marshalled, periods.begin(), periods.end());
    ASSERT_EQ(out, marshalled + PacketPeriods::MIN_SIZE + PacketPeriods::PERIOD_SIZE * 2);
    ASSERT_FALSE(std::memcmp(expected, marshalled, sizeof(marshalled)));
}

TEST(protocol_PacketPeriods, unmarshal)
{
    constexpr int PACKET_SIZE = PacketPeriods::MIN_SIZE + PacketPeriods::PERIOD_SIZE * 2;
    uint8_t marshalled[PACKET_SIZE] = {
        0x1, 0x1,
        0x1, 0x1, 0x2, 0x3, 0x4,
        0x2, 0x5, 0x6, 0x7, 0x8
    };

    PacketPeriods::Periods periods {
        { 1, 0x04030201 },
        { 2, 0x08070605 }
    };

    auto unmarshalled = PacketPeriods::unmarshal(marshalled, marshalled + PACKET_SIZE);
    ASSERT_EQ(2, unmarshalled.size());
    ASSERT_EQ(*periods.begin(), *unmarshalled.begin());
    ASSERT_EQ(*periods.rbegin(), *unmarshalled.rbegin());
}

TEST(protocol_PacketPeriods, unmarshal_throws_if_too_little_data_is_given)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(PacketPeriods::unmarshal(ptr, ptr + PacketPeriods::MIN_SIZE - 1), std::length_error);
}

TEST(protocol_PacketPeriods, unmarshal_throws_if_the_buffer_size_is_not_an_integral_number_of_periods)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(PacketPeriods::unmarshal(ptr, ptr + PacketPeriods::MIN_SIZE - PacketPeriods::PERIOD_SIZE - 1), std::length_error);
}

static_assert(sizeof(BaudRates) == BaudRates::SIZE, "sizeof and SIZE do not agree");

TEST(protocol_BaudRates, marshal)
{
    BaudRates data;
    data.permanent = 1;
    data.primary_port = 0x04030201;
    data.gpio = 0x08070605;
    data.auxiliary_rs232 = 0x0c0b0a09;
    uint8_t expected[] = { 1, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0, 0, 0, 0 };

    uint8_t marshalled[BaudRates::SIZE];
    auto out = data.marshal(marshalled);
    ASSERT_EQ(out, marshalled + BaudRates::SIZE);
    ASSERT_FALSE(std::memcmp(expected, marshalled, BaudRates::SIZE));
}

TEST(protocol_BaudRates, unmarshal)
{
    uint8_t marshalled[] =
    { 1, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0, 0, 0, 0 };

    BaudRates expected;
    expected.permanent = 0;
    expected.primary_port = 0x04030201;
    expected.gpio = 0x08070605;
    expected.auxiliary_rs232 = 0x0c0b0a09;
    expected.reserved = 0;

    BaudRates actual = BaudRates::unmarshal(marshalled, marshalled + BaudRates::SIZE);
    ASSERT_FALSE(std::memcmp(&actual, &expected, BaudRates::SIZE));
}

TEST(protocol_BaudRates, unmarshal_fails_if_too_little_data_is_provided_and_does_not_access_any_of_it)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(BaudRates::unmarshal(ptr, ptr + BaudRates::SIZE - 1), std::length_error);
}

TEST(protocol_BaudRates, unmarshal_fails_if_too_much_data_is_provided_and_does_not_access_any_of_it)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(BaudRates::unmarshal(ptr, ptr + BaudRates::SIZE + 1), std::length_error);
}


static_assert(sizeof(Alignment) == Alignment::SIZE, "sizeof and SIZE do not agree");

TEST(protocol_Alignment, marshal)
{
    Alignment data;
    data.permanent = 1;
    for (int i = 0; i < 9; ++i)
        data.dcm[i] = TEST_FP4[i].fp;
    for (int i = 0; i < 3; ++i)
    {
        data.gnss_antenna_offset_xyz[i]  = TEST_FP4[9 + i].fp;
        data.odometer_offset_xyz[i]      = TEST_FP4[i].fp;
        data.external_data_offset_xyz[i] = TEST_FP4[3 + i].fp;
    }

    uint8_t expected[Alignment::SIZE];
    expected[0] = 1;
    for (int i = 0; i < 18; ++i)
        RAW_SET(expected + 1 + i * 4, TEST_FP4[i % 12].binary);

    uint8_t marshalled[Alignment::SIZE];
    auto out = data.marshal(marshalled);
    ASSERT_EQ(out, marshalled + Alignment::SIZE);
    ASSERT_FALSE(std::memcmp(expected, marshalled, Alignment::SIZE));
}

TEST(protocol_Alignment, unmarshal)
{
    Alignment expected;
    expected.permanent = 0;
    for (int i = 0; i < 9; ++i)
        expected.dcm[i] = TEST_FP4[i].fp;
    for (int i = 0; i < 3; ++i)
    {
        expected.gnss_antenna_offset_xyz[i]  = TEST_FP4[9 + i].fp;
        expected.odometer_offset_xyz[i]      = TEST_FP4[i].fp;
        expected.external_data_offset_xyz[i] = TEST_FP4[3 + i].fp;
    }

    uint8_t marshalled[Alignment::SIZE];
    marshalled[0] = 1;
    for (int i = 0; i < 18; ++i)
        RAW_SET(marshalled + 1 + i * 4, TEST_FP4[i % 12].binary);

    Alignment out = Alignment::unmarshal(marshalled, marshalled + Alignment::SIZE);
    ASSERT_FALSE(std::memcmp(&expected, &out, Alignment::SIZE));
}

TEST(protocol_Alignment, unmarshal_throws_if_given_too_little_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(Alignment::unmarshal(ptr, ptr + Alignment::SIZE - 1), std::length_error);
}

TEST(protocol_Alignment, unmarshal_throws_if_given_too_much_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(Alignment::unmarshal(ptr, ptr + Alignment::SIZE + 1), std::length_error);
}

static_assert(sizeof(FilterOptions) == FilterOptions::SIZE, "sizeof and SIZE do not agree");

TEST(protocol_FilterOptions, marshal)
{
    FilterOptions data;
    data.permanent                    = 1;
    data.vehicle_type                 = 2;
    data.enabled_internal_gnss        = 3;
    data.reserved_0                   = 0;
    data.enabled_atmospheric_altitude = 5;
    data.enabled_velocity_heading     = 6;
    data.enabled_reversing_detection  = 7;
    data.enabled_motion_analysis      = 8;

    uint8_t expected[FilterOptions::SIZE] =
    { 1, 2, 3, 0, 5, 6, 7, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    uint8_t marshalled[FilterOptions::SIZE];
    auto out = data.marshal(marshalled);
    ASSERT_EQ(out, marshalled + FilterOptions::SIZE);
    ASSERT_FALSE(std::memcmp(expected, marshalled, FilterOptions::SIZE));
}

TEST(protocol_FilterOptions, unmarshal)
{
    FilterOptions expected;
    expected.permanent                    = 0;
    expected.vehicle_type                 = 2;
    expected.enabled_internal_gnss        = 3;
    expected.reserved_0                   = 0;
    expected.enabled_atmospheric_altitude = 5;
    expected.enabled_velocity_heading     = 6;
    expected.enabled_reversing_detection  = 7;
    expected.enabled_motion_analysis      = 8;

    uint8_t marshalled[FilterOptions::SIZE] =
    { 1, 2, 3, 0, 5, 6, 7, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    FilterOptions out = FilterOptions::unmarshal(
            marshalled, marshalled + FilterOptions::SIZE);
    ASSERT_FALSE(std::memcmp(&expected, &out, FilterOptions::SIZE));
}

TEST(protocol_FilterOptions, unmarshal_throws_if_given_too_little_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(FilterOptions::unmarshal(ptr, ptr + FilterOptions::SIZE - 1), std::length_error);
}

TEST(protocol_FilterOptions, unmarshal_throws_if_given_too_much_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(FilterOptions::unmarshal(ptr, ptr + FilterOptions::SIZE + 1), std::length_error);
}

static_assert(sizeof(MagneticCalibrationValues) == MagneticCalibrationValues::SIZE, "sizeof and SIZE do not agree");

TEST(protocol_MagneticCalibrationValues, marshal)
{
    MagneticCalibrationValues data;
    data.permanent = 1;
    for (int i = 0; i < 3; ++i)
        data.hard_iron_bias_xyz[i] = TEST_FP4[i].fp;
    for (int i = 0; i < 9; ++i)
        data.soft_iron_transformation[i] = TEST_FP4[3 + i].fp;

    uint8_t expected[MagneticCalibrationValues::SIZE];
    expected[0] = 1;
    for (int i = 0; i < 12; ++i)
        RAW_SET(expected + 1 + i * 4, TEST_FP4[i].binary);

    uint8_t marshalled[MagneticCalibrationValues::SIZE];
    auto out = data.marshal(marshalled);
    ASSERT_EQ(out, marshalled + MagneticCalibrationValues::SIZE);
    ASSERT_FALSE(std::memcmp(expected, marshalled, MagneticCalibrationValues::SIZE));
}

TEST(protocol_MagneticCalibrationValues, unmarshal)
{
    MagneticCalibrationValues expected;
    expected.permanent = 0;
    for (int i = 0; i < 3; ++i)
        expected.hard_iron_bias_xyz[i] = TEST_FP4[i].fp;
    for (int i = 0; i < 9; ++i)
        expected.soft_iron_transformation[i] = TEST_FP4[3 + i].fp;

    uint8_t marshalled[MagneticCalibrationValues::SIZE];
    marshalled[0] = 1;
    for (int i = 0; i < 12; ++i)
        RAW_SET(marshalled + 1 + i * 4, TEST_FP4[i].binary);

    MagneticCalibrationValues out = MagneticCalibrationValues::unmarshal(
            marshalled, marshalled + MagneticCalibrationValues::SIZE);
    ASSERT_FALSE(std::memcmp(&expected, &out, MagneticCalibrationValues::SIZE));
}

TEST(protocol_MagneticCalibrationValues, unmarshal_throws_if_given_too_little_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(MagneticCalibrationValues::unmarshal(ptr, ptr + MagneticCalibrationValues::SIZE - 1), std::length_error);
}

TEST(protocol_MagneticCalibrationValues, unmarshal_throws_if_given_too_much_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(MagneticCalibrationValues::unmarshal(ptr, ptr + MagneticCalibrationValues::SIZE + 1), std::length_error);
}

static_assert(sizeof(MagneticCalibrationConfiguration) == MagneticCalibrationConfiguration::SIZE, "sizeof and SIZE do not agree");

TEST(protocol_MagneticCalibrationConfiguration, marshal)
{
    MagneticCalibrationConfiguration data;
    data.action = 1;

    uint8_t expected[MagneticCalibrationConfiguration::SIZE];
    expected[0] = 1;

    uint8_t marshalled[MagneticCalibrationConfiguration::SIZE];
    auto out = data.marshal(marshalled);
    ASSERT_EQ(out, marshalled + MagneticCalibrationConfiguration::SIZE);
    ASSERT_FALSE(std::memcmp(expected, marshalled, MagneticCalibrationConfiguration::SIZE));
}

static_assert(sizeof(MagneticCalibrationStatus) == MagneticCalibrationStatus::SIZE, "sizeof and SIZE do not agree");

TEST(protocol_MagneticCalibrationStatus, unmarshal)
{
    MagneticCalibrationStatus expected = { 1, 2, 3 };
    uint8_t marshalled[MagneticCalibrationStatus::SIZE] = { 1, 2, 3 };

    MagneticCalibrationStatus out = MagneticCalibrationStatus::unmarshal(
            marshalled, marshalled + MagneticCalibrationStatus::SIZE);
    ASSERT_FALSE(std::memcmp(&expected, &out, MagneticCalibrationStatus::SIZE));
}

TEST(protocol_MagneticCalibrationStatus, unmarshal_throws_if_given_too_little_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(MagneticCalibrationStatus::unmarshal(ptr, ptr + MagneticCalibrationStatus::SIZE - 1), std::length_error);
}

TEST(protocol_MagneticCalibrationStatus, unmarshal_throws_if_given_too_much_data)
{
    uint8_t* ptr = nullptr;
    ASSERT_THROW(MagneticCalibrationStatus::unmarshal(ptr, ptr + MagneticCalibrationStatus::SIZE + 1), std::length_error);
}

struct protocol_FunctionsTest : DriverTestBase
{
    protocol_FunctionsTest()
    {
        openTestURI();
    }
};


struct PacketMock {
    static constexpr uint8_t ID   = 5;
    static constexpr uint8_t SIZE = 4;

    uint8_t buffer[SIZE];

    PacketMock()
    {
        std::iota(buffer, buffer + PacketMock::SIZE, 0);
    }

    template<typename OutputIterator>
    OutputIterator marshal(OutputIterator it) const
    {
        return std::copy_n(buffer, SIZE, it);
    }

    template<typename InputIterator>
    static PacketMock unmarshal(InputIterator begin, InputIterator end)
    {
        PacketMock packet;
        std::copy(begin, end, packet.buffer);
        return packet;
    }
} __attribute__((packed));

TEST_F(protocol_FunctionsTest, writePacket_marshals_adds_a_valid_header_and_writes_the_packet)
{
    PacketMock packet;

    Header header = imu_advanced_navigation_anpp::protocol::writePacket(driver, packet);
    auto raw_data = readDataFromDriver();
    ASSERT_FALSE(std::memcmp(raw_data.data(), &header, 5));
    ASSERT_FALSE(std::memcmp(raw_data.data() + 5, &packet, 4));

    ASSERT_TRUE(header.isValid());
    ASSERT_TRUE(header.isPacketValid(&raw_data[5], &raw_data[9]));
    ASSERT_EQ(header.packet_id, 5);
    ASSERT_EQ(header.payload_length, 4);
}

TEST_F(protocol_FunctionsTest, waitForPacket_returns_if_it_receives_the_expected_packet_within_the_timeout)
{
    std::vector<uint8_t> packet_buffer{ 0, 1, 2, 3 };
    pushDataToDriver(makePacket<PacketMock>(packet_buffer));
    auto packet = waitForPacket<PacketMock>(driver, base::Time());

    EXPECT_THAT(std::vector<uint8_t>(packet.buffer, packet.buffer + 4), ContainerEq(packet_buffer));
}

TEST_F(protocol_FunctionsTest, waitForPacket_throws_if_no_packets_have_been_received)
{
    std::vector<uint8_t> packet_buffer{ 0, 1, 2, 3 };
    ASSERT_THROW(waitForPacket<PacketMock>(driver, base::Time()), iodrivers_base::TimeoutError);
}

TEST_F(protocol_FunctionsTest, waitForPacket_ignores_packets_with_a_non_matching_ID_and_return_the_right_packet)
{
    pushDataToDriver(makePacket<BootMode>({ 3 }));
    std::vector<uint8_t> packet_buffer{ 0, 1, 2, 3 };
    pushDataToDriver(makePacket<PacketMock>(packet_buffer));
    auto packet = waitForPacket<PacketMock>(driver, base::Time::fromSeconds(1));

    EXPECT_THAT(std::vector<uint8_t>(packet.buffer, packet.buffer + 4), ContainerEq(packet_buffer));
}

TEST_F(protocol_FunctionsTest, waitForPacket_accounts_for_time_spent_reading_ignored_packets_in_the_overall_timeout)
{
    for (int i = 0; i < 1000; ++i)
        pushDataToDriver(makePacket<BootMode>({ 3 }));
    pushDataToDriver(makePacket<PacketMock>({ 0, 1, 2, 3 }));
    // Timeout(Time()) is infinite timeout ... go f... figure
    ASSERT_THROW(waitForPacket<PacketMock>(driver, base::Time::fromMicroseconds(1)), iodrivers_base::TimeoutError);
}

TEST_F(protocol_FunctionsTest, waitForAck_returns_an_ack_that_matches_the_given_header)
{
    Header header;
    header.packet_id = 1;
    header.payload_checksum_lsb = 2;
    header.payload_checksum_msb = 3;

    pushDataToDriver(makePacket<Acknowledge>(
                { 1, 2, 3, static_cast<uint8_t>(imu_advanced_navigation_anpp::ACK_FAILED_UNKNOWN_PACKET) }));
    ASSERT_EQ(imu_advanced_navigation_anpp::ACK_FAILED_UNKNOWN_PACKET, waitForAck(driver, header, base::Time()));
}

TEST_F(protocol_FunctionsTest, waitForAck_ignores_acks_for_other_packets)
{
    Header header;
    header.packet_id = 1;
    header.payload_checksum_lsb = 2;
    header.payload_checksum_msb = 3;

    pushDataToDriver(makePacket<Acknowledge>(
                { 1, 2, 4, static_cast<uint8_t>(imu_advanced_navigation_anpp::ACK_FAILED_UNKNOWN_PACKET) }));
    pushDataToDriver(makePacket<Acknowledge>(
                { 1, 2, 3, static_cast<uint8_t>(imu_advanced_navigation_anpp::ACK_SUCCESS) }));
    ASSERT_EQ(imu_advanced_navigation_anpp::ACK_SUCCESS, waitForAck(driver, header, base::Time()));
}

TEST_F(protocol_FunctionsTest, waitForAck_accounts_for_time_spent_reading_ignored_acks_in_the_overall_timeout)
{
    Header header;
    header.packet_id = 1;
    header.payload_checksum_lsb = 2;
    header.payload_checksum_msb = 3;

    // Timeout(Time()) is infinite timeout ... go f... figure
    for (int i = 0; i < 1000; ++i)
        pushDataToDriver(makePacket<Acknowledge>(
                    { 1, 2, 4, static_cast<uint8_t>(imu_advanced_navigation_anpp::ACK_FAILED_UNKNOWN_PACKET) }));
    pushDataToDriver(makePacket<Acknowledge>(
                { 1, 2, 3, static_cast<uint8_t>(imu_advanced_navigation_anpp::ACK_SUCCESS) }));
    ASSERT_THROW(waitForAck(driver, header, base::Time::fromMicroseconds(1)), iodrivers_base::TimeoutError);
}

TEST_F(protocol_FunctionsTest, validateAck_returns_if_an_ack_is_received_with_ACK_SUCCESS)
{
    Header header;
    header.packet_id = 1;
    header.payload_checksum_lsb = 2;
    header.payload_checksum_msb = 3;
    pushDataToDriver(makePacket<Acknowledge>(
                { 1, 2, 3, static_cast<uint8_t>(imu_advanced_navigation_anpp::ACK_SUCCESS) }));
    EXPECT_NO_THROW(validateAck(driver, header, base::Time()));
}

TEST_F(protocol_FunctionsTest, validateAck_throws_AcknowledgeFailure_if_an_ack_is_received_with_a_failure)
{
    Header header;
    header.packet_id = 1;
    header.payload_checksum_lsb = 2;
    header.payload_checksum_msb = 3;
    pushDataToDriver(makePacket<Acknowledge>(
                { 1, 2, 3, static_cast<uint8_t>(imu_advanced_navigation_anpp::ACK_FAILED_UNKNOWN_PACKET) }));
    ASSERT_THROW(validateAck(driver, header, base::Time()), imu_advanced_navigation_anpp::AcknowledgeFailure);
}

