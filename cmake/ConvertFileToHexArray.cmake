function(file_to_hex_array FILE DATA)
  # Read the ASCII file as hex.
  file(READ "${FILE}" HEX_CONTENTS HEX)

  # Separate into individual bytes.
  string(REGEX MATCHALL "([A-Za-z0-9][A-Za-z0-9])" SEPARATED_HEX "${HEX_CONTENTS}")

  # Append the "0x" to each byte.
  list(JOIN SEPARATED_HEX ", 0x" FORMATTED_HEX)

  # JOIN misses the first byte's "0x", so add it here.
  string(PREPEND FORMATTED_HEX "0x")

  # Set the variable named by DATA argument to the formatted hex string.
  set(${DATA} ${FORMATTED_HEX} PARENT_SCOPE)
endfunction()
