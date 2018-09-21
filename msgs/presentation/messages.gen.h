enum UrlAvailability {
  kNotCompatible = 0,
  kCompatible = 1,
  kNotValid = 10,
};

struct UrlAvailabilityRequest {
  uint64_t request_id;
  std::vector<std::string> urls;
};

struct UrlAvailabilityResponse {
  uint64_t request_id;
  uint64_t timestamp;
  std::vector<UrlAvailability> url_availabilities;
};

ssize_t EncodeUrlAvailabilityRequest(
    const UrlAvailabilityRequest& data,
    uint8_t* buffer,
    size_t length);
