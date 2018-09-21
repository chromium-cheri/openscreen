ssize_t EncodeUrlAvailabilityRequest(const UrlAvailabilityRequest& data,
                                     uint8_t* buffer,
                                     size_t length) {
  CborEncoder encoder0;
  cbor_encoder_init(&encoder0, buffer, length, 0);
  CBOR_RETURN_ON_ERROR(cbor_encode_tag(
      &encoder0, static_cast<uint64_t>(Tag::kUrlAvailabilityRequest)));
  CborEncoder encoder1;
  CBOR_RETURN_ON_ERROR(cbor_encoder_create_map(&encoder0, &encoder1, 2));
  CBOR_RETURN_ON_ERROR(cbor_encode_text_string(&encoder1, "request-id",
                                               sizeof("request-id") - 1));
  CBOR_RETURN_ON_ERROR(cbor_encode_uint(&encoder1, data.request_id));
  CBOR_RETURN_ON_ERROR(
      cbor_encode_text_string(&encoder1, "urls", sizeof("urls") - 1));
  CborEncoder encoder2;
  CBOR_RETURN_ON_ERROR(
      cbor_encoder_create_array(&encoder1, &encoder2, data.urls.size()));
  for (const auto& x : data.urls) {
    CBOR_RETURN_ON_ERROR(IsValidUtf8(x));
    CBOR_RETURN_ON_ERROR(
        cbor_encode_text_string(&encoder2, x.c_str(), x.size()));
  }
  CBOR_RETURN_ON_ERROR(cbor_encoder_close_container(&encoder1, &encoder2));
  CBOR_RETURN_ON_ERROR(cbor_encoder_close_container(&encoder0, &encoder1));
  size_t extra_bytes_needed = cbor_encoder_get_extra_bytes_needed(&encoder0);
  if (extra_bytes_needed) {
    return static_cast<ssize_t>(length + extra_bytes_needed);
  } else {
    return static_cast<ssize_t>(
        cbor_encoder_get_buffer_size(&encoder0, buffer));
  }
}
