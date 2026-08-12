/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Embedded test certificate and RSA private key used by the
 * server fuzz target and its seed generator.
 *
 * This is a throwaway, self-signed test certificate used only to
 * make the FreeRDP server core accept a connection. It is not a
 * secret and must not be used anywhere outside of tests.
 *
 * Copyright 2026 Thincast Technologies GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FREERDP_CORE_TEST_TEST_FUZZ_SERVER_CERTS_H
#define FREERDP_CORE_TEST_TEST_FUZZ_SERVER_CERTS_H

static const char test_server_cert_pem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDCzCCAfOgAwIBAgIUJ4fYw1jTmLQQhnFwryzcBeAok6cwDQYJKoZIhvcNAQEL\n"
    "BQAwFDESMBAGA1UEAwwJZnV6ei10ZXN0MCAXDTI2MDgxMTE2MzE1NloYDzIxMjYw\n"
    "NzE4MTYzMTU2WjAUMRIwEAYDVQQDDAlmdXp6LXRlc3QwggEiMA0GCSqGSIb3DQEB\n"
    "AQUAA4IBDwAwggEKAoIBAQDW3lJNI+ibC7rKUDiC2iqT8fG8L5R03tEsS4fr5uQy\n"
    "iBAFWf1vq8Uyx95QA5sPGpA/LRf7Kg2M1t3B5PWYhG+4VnhNGWF2Zo76wp8W32kX\n"
    "MM65QJ2798AV8+QO+HgH7bcH1UbbNbJFLre4bFIvEo+rUDvOC1P9pYxvnTHOBgzH\n"
    "Y+sEh903YMAPrE9wEAgh2vd1Knl3YWYvbyzMU1mNQ8ZSvCnnR965TS2NN4rXs2Kw\n"
    "dMrjcvrL4e8SiJMEz7A7YqNRHvnWgWv6XCYdFS+EE3LQ5Sgng4DawFv46mLiNP7A\n"
    "QFqm8uTcSmb0OsENRQC2HgdrcnfCZq/lg1jqRmC/q/TdAgMBAAGjUzBRMB0GA1Ud\n"
    "DgQWBBQsvO2OvylJ5rlzvMefx2+WRBwPxDAfBgNVHSMEGDAWgBQsvO2OvylJ5rlz\n"
    "vMefx2+WRBwPxDAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3DQEBCwUAA4IBAQAW\n"
    "ogjIDaWtMwSwnHxxTmMoKLtNOE+8ZNXTRat903Ro0RGz4F72R+UuI7MEApLn27E0\n"
    "YQYuwR8++5Kc0EKW9inUtMrXFFIEg3FxVcNWPwHZdIm2YsYM7SMEnzgAi58YAwhh\n"
    "PqjV0dxm4N4nQkdTFAEVE9sO7isEsEzC/LQC+qRnKZm2QEsS0bLuGBFnRuipOedN\n"
    "peLAbcWIO5XaStfGEhPGUdCOU5pmqJGx/otYNKHhpS6uKvZAIw26yvaOPq8f72UN\n"
    "QmA9qGipJBjuG7yY626DqUqt5cHUumZ24TENreVgc31nPehb7wx6qPqYIR24uTDy\n"
    "UKRjI3r580PJGTsDj7y1\n"
    "-----END CERTIFICATE-----\n";

static const char test_server_key_pem[] = "-----BEGIN PRIVATE KEY-----\n"
    "MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQDW3lJNI+ibC7rK\n"
    "UDiC2iqT8fG8L5R03tEsS4fr5uQyiBAFWf1vq8Uyx95QA5sPGpA/LRf7Kg2M1t3B\n"
    "5PWYhG+4VnhNGWF2Zo76wp8W32kXMM65QJ2798AV8+QO+HgH7bcH1UbbNbJFLre4\n"
    "bFIvEo+rUDvOC1P9pYxvnTHOBgzHY+sEh903YMAPrE9wEAgh2vd1Knl3YWYvbyzM\n"
    "U1mNQ8ZSvCnnR965TS2NN4rXs2KwdMrjcvrL4e8SiJMEz7A7YqNRHvnWgWv6XCYd\n"
    "FS+EE3LQ5Sgng4DawFv46mLiNP7AQFqm8uTcSmb0OsENRQC2HgdrcnfCZq/lg1jq\n"
    "RmC/q/TdAgMBAAECggEAFippqZkMeCAp5RCI/+CzNz9kjWIIKdVJlUz2aNzRCjB0\n"
    "nKS3qxM4fOBW/ACfOJvoKQhFGs0wCCkrR8MPnev9nXHYJ7X4Uq9KTS6SHFkwPWr0\n"
    "zHIQw5EPkQQvsOardT/t24JCNL9xlEb5P253PPFofkcA4GTVRYuUNPhtqKABpfjl\n"
    "lVhwnqwCvRb/R/udWidCTZ5Kp088Fzh2aQ52vK4KDO70PqfR2Jze20qMB9Kj6yCT\n"
    "t6SlKIi7SjQv7CXjlMtiMppwiesls3aeE8MGt8ildDE2Xyw5M8SoUWxPVNzmkJbF\n"
    "YmFauNaJzbE05i7WCm7PpXhZux+SL9bfZrpty8RJmQKBgQD2VHu+2a5hqPqk1f7x\n"
    "BErPqFzHY9ozIY5pXzr0yfIEg0xGsZFm6uGRkfmDd6G67+OytNXexisidu9eaxOL\n"
    "0i+nMbW7iV8v+h7hrmx6PngY7mY/wrkqihWNUyXfEjPxr//hyGH9+Fua09Vg+b1A\n"
    "WBwIyrZTbqtUVMgXH56xvyJfaQKBgQDfTal/uUd/FbgKzNk3YsIhWTvpgfoWZq4p\n"
    "Fc0FO2ZUDGGwzng+yIO7GdAfAiMGxnvDWGTnzIDWgxmrVZzDCsERAY7u5wgQkJvf\n"
    "fx3/ZD7KAGJkfkBcDVQZ5Jdg/3z/VmxNeDwPY8uO3Fd0yv/OLZB4cyns9/GRytWB\n"
    "+FPPcg4vVQKBgQCFEDoQbHKAmtFafabL9y+aYS5NHyldeYD+dszYMsajnXF0trL+\n"
    "z16uThZk6BjbbH6pqHnnb1EZuvmvHVRfsVjAjl/HQHvE5O4NpzU+C8TAYvek9cEk\n"
    "s5bU0tegWqroodQt2RrmIGULi+a2DfIncfEi5q36/8tZMLstko0dI0ykEQKBgBWt\n"
    "Wlj1yYUCvLz/qc6AncvS98fxQC/Qg/OlFCP/4i0ijpE1WeLuYCtXlCaOdIwB1J3g\n"
    "BNujtJYeX+2MAA3HC3r1JcT3VIcXIqqNkoHqX1YIt4R95Q2KlbF1yWQ3KRE4eIcE\n"
    "tv/fdjFGHo9N7Ys8TRwEQfupDiBTCmr1il1G+y2JAoGBAI9wl+3HaLSLZlnOOG+4\n"
    "P5WpaGAXbkWMcLq01Wj6wqhlxRg9jugd9+1DjQ7dcmFWKUEOvyxPvl9wsoBuZMAn\n"
    "EBHFJ1KB5jBtcEkH8HdAhErbIoun294YwsYPgHbBfzuz+B6BSoRFprt0QC8LFQdJ\n"
    "UXcX6dcIPbIz/HHap0MWmHXW\n"
    "-----END PRIVATE KEY-----\n";

#endif /* FREERDP_CORE_TEST_TEST_FUZZ_SERVER_CERTS_H */
