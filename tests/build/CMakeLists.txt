# Tests of the build itself: that the settings a preset turns on take hold. They belong to no target,
# so they are named after the build.

if("address" IN_LIST APLOMB_SANITIZERS)
    aplomb_add_program_test(build "AddressSanitizer stops a read past the end of a heap allocation"
        SOURCE programs/heap_overflow.cpp
        EXPECTED_RESULT 1
        STDERR_MATCHES "ERROR: AddressSanitizer: heap-buffer-overflow"
    )
endif()

if("undefined" IN_LIST APLOMB_SANITIZERS)
    aplomb_add_program_test(build "UndefinedBehaviorSanitizer stops a signed integer overflow"
        SOURCE programs/signed_overflow.cpp
        EXPECTED_RESULT 1
        STDERR_MATCHES "runtime error: signed integer overflow"
    )
endif()

if(NOT APLOMB_EXCEPTIONS)
    aplomb_add_compile_failure_test(build "the library's settings reject a throw when exceptions are off"
        SOURCE programs/stray_throw.cpp
        # GCC's message, then Clang's.
        MATCHES "exception handling disabled|cannot use 'throw' with exceptions disabled"
    )
endif()

# The licence check's verdicts on licence expressions, against the allowlist.
function(aplomb_add_license_verdict_test description expression expected)
    aplomb_add_script_test(build "${description}"
        SCRIPT licenses/verdict.cmake
        DEFINITIONS "EXPRESSION=${expression}" "EXPECTED=${expected}"
    )
endfunction()

aplomb_add_license_verdict_test("a licence on the allowlist is allowed" "MIT" allowed)
aplomb_add_license_verdict_test("a licence off the allowlist is refused" "GPL-3.0-only" refused)
aplomb_add_license_verdict_test("a port that declares no licence is undecided" "NOASSERTION" undecided)
aplomb_add_license_verdict_test("an empty licence is undecided" "" undecided)
aplomb_add_license_verdict_test("a licence of the port's own is undecided" "LicenseRef-Proprietary" undecided)
aplomb_add_license_verdict_test("licences that all apply are allowed when all are allowed"
    "MIT AND Zlib" allowed
)
aplomb_add_license_verdict_test("licences that all apply are refused when one is refused"
    "MIT AND GPL-3.0-only" refused
)
aplomb_add_license_verdict_test("licences that all apply are refused when one is refused and another undecided"
    "LicenseRef-Custom AND GPL-3.0-only" refused
)
aplomb_add_license_verdict_test("licences that all apply are undecided when one is undecided"
    "MIT AND LicenseRef-Custom" undecided
)
aplomb_add_license_verdict_test("a choice of licences is allowed when one is allowed"
    "GPL-3.0-only OR MIT" allowed
)
aplomb_add_license_verdict_test("a choice of licences is refused when all are refused"
    "GPL-3.0-only OR LGPL-2.1-only" refused
)
aplomb_add_license_verdict_test("a choice of licences is undecided when none is allowed and one is undecided"
    "GPL-3.0-only OR LicenseRef-Custom" undecided
)
aplomb_add_license_verdict_test("a licence with an exception on the allowlist is allowed"
    "Apache-2.0 WITH LLVM-exception" allowed
)
aplomb_add_license_verdict_test("an allowed licence with an exception off the allowlist is undecided"
    "MIT WITH Some-exception" undecided
)
aplomb_add_license_verdict_test("a refused licence with an exception is refused"
    "GPL-2.0-only WITH Classpath-exception-2.0" refused
)
aplomb_add_license_verdict_test("an expression in parentheses is decided first"
    "(MIT OR GPL-3.0-only) AND LGPL-2.1-only" refused
)
aplomb_add_license_verdict_test("AND binds more tightly than OR"
    "MIT OR GPL-3.0-only AND LGPL-2.1-only" allowed
)
aplomb_add_license_verdict_test("an expression with an operator missing its operand is undecided"
    "MIT AND" undecided
)
aplomb_add_license_verdict_test("an expression with an unclosed parenthesis is undecided"
    "(MIT OR Zlib" undecided
)
aplomb_add_license_verdict_test("an expression of two licences with no operator is undecided"
    "MIT Zlib" undecided
)
aplomb_add_license_verdict_test("an expression with an exception but no licence is undecided"
    "WITH LLVM-exception" undecided
)

# The licence check on the trees `vcpkg install` leaves, faked by licenses/fake_vcpkg.cmake.
aplomb_add_script_test(build "configuring fails, naming every port whose licence is refused"
    SCRIPT licenses/refused_licenses.cmake
    EXPECTED_RESULT 1
    STDERR_MATCHES "agpl-tool:x64-linux is under AGPL-3.0-only.*gpl-lib:x64-linux is under GPL-3.0-only"
)
aplomb_add_script_test(build "configuring fails, naming every port whose licence a person has to read"
    SCRIPT licenses/undecided_licenses.cmake
    EXPECTED_RESULT 1
    STDERR_MATCHES "bare-lib:x64-linux is under NOASSERTION, which the check can't decide.*odd-lib:x64-linux is under LicenseRef-Odd, which the check can't decide"
)
aplomb_add_script_test(build "a licence the check can't decide passes once a person has reviewed it"
    SCRIPT licenses/undecided_licenses.cmake
    DEFINITIONS "REVIEWED=bare-lib=NOASSERTION,odd-lib=LicenseRef-Odd"
)
aplomb_add_script_test(build "a review of a port's licence covers no other licence of the port"
    SCRIPT licenses/undecided_licenses.cmake
    DEFINITIONS "REVIEWED=bare-lib=NOASSERTION,odd-lib=LicenseRef-Older"
    EXPECTED_RESULT 1
    STDERR_MATCHES "odd-lib:x64-linux is under LicenseRef-Odd, which the check can't decide"
)
aplomb_add_script_test(build "the notices give each shipped port's version, licence, homepage and licence text"
    SCRIPT licenses/notices.cmake
)
aplomb_add_script_test(build "the notices list what overlay ports ship, but not their host tools or test-only ports"
    SCRIPT licenses/shipped_ports.cmake
    DEFINITIONS REGISTRY=OFF
)
aplomb_add_script_test(build "the notices list what registry ports ship, but not their host tools or test-only ports"
    SCRIPT licenses/shipped_ports.cmake
    DEFINITIONS REGISTRY=ON
)
aplomb_add_script_test(build "a port vcpkg has since removed isn't checked"
    SCRIPT licenses/status_updates.cmake
    DEFINITIONS INSTALLED=MIT
)
aplomb_add_script_test(build "a port vcpkg installed since it last wrote its status is checked"
    SCRIPT licenses/status_updates.cmake
    DEFINITIONS INSTALLED=GPL-2.0-only
    EXPECTED_RESULT 1
    STDERR_MATCHES "new-lib:x64-linux is under GPL-2.0-only, which the allowlist refuses"
)
foreach(missing IN ITEMS document licence)
    aplomb_add_script_test(build "a port whose SPDX ${missing} is missing has a licence the check can't decide"
        SCRIPT licenses/missing_metadata.cmake
        DEFINITIONS MISSING=${missing} REVIEWED=OFF
        EXPECTED_RESULT 1
        STDERR_MATCHES "bare-lib:x64-linux is under NOASSERTION, which the check can't decide"
    )
    aplomb_add_script_test(build "a reviewed port whose SPDX ${missing} is missing is in the notices"
        SCRIPT licenses/missing_metadata.cmake
        DEFINITIONS MISSING=${missing} REVIEWED=ON
    )
endforeach()

if(DEFINED APLOMB_THIRD_PARTY_NOTICES)
    add_test(NAME "build: the committed third-party notices are the ones vcpkg's metadata gives"
        COMMAND "${CMAKE_COMMAND}" -E compare_files --ignore-eol
            "${APLOMB_THIRD_PARTY_NOTICES}" "${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
    )
endif()
