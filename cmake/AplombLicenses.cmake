# The licences aplomb accepts from its dependencies.

include_guard(GLOBAL)

# The licences a dependency may carry, build-time tools included, as SPDX licence expressions.
set(APLOMB_ALLOWED_LICENSES
    Zlib
    MIT
    BSD-2-Clause
    BSD-3-Clause
    BSL-1.0
    ISC
    Apache-2.0
    "Apache-2.0 WITH LLVM-exception"
    CC0-1.0
    Unlicense
)

# The licences the check can't decide that a person has read and found within the allowlist, each as
# <port>=<licence expression>. A review covers that one expression, so a port whose licence changes
# is checked again.
set(APLOMB_REVIEWED_LICENSES "")

# Sets out_verdict to the verdict on an SPDX licence expression: `allowed`, `refused`, or
# `undecided` when the allowlist alone can't say and a person has to read the licence. A port that
# declares no licence, which vcpkg writes as NOASSERTION, a licence of the port's own, an allowed
# licence with an exception the allowlist doesn't name, and an expression that doesn't parse are all
# undecided.
#
# Where every licence of an AND applies, a refused one refuses the whole; where an OR offers a
# choice, an allowed one allows it. AND binds more tightly than OR, as SPDX defines.
function(aplomb_license_verdict expression out_verdict)
    string(REGEX MATCHALL "[()]|[^ \t\r\n()]+" tokens "${expression}")
    list(LENGTH tokens count)
    set(position 0)
    set(malformed FALSE)
    _aplomb_license_or()
    if(malformed OR count EQUAL 0 OR position LESS count)
        set(verdict undecided)
    endif()
    set(${out_verdict} ${verdict} PARENT_SCOPE)
endfunction()

# The parser behind aplomb_license_verdict. Each function reads its part of the expression from
# `tokens`, starting at `position`, and sets `verdict` and the new `position` in the caller's scope.
# A part that doesn't parse sets `malformed`.

# Reads licences joined by OR.
function(_aplomb_license_or)
    _aplomb_license_and()
    set(result ${verdict})
    while(NOT malformed AND position LESS count)
        list(GET tokens ${position} token)
        if(NOT token STREQUAL "OR")
            break()
        endif()
        math(EXPR position "${position} + 1")
        _aplomb_license_and()
        if(result STREQUAL "allowed" OR verdict STREQUAL "allowed")
            set(result allowed)
        elseif(result STREQUAL "undecided" OR verdict STREQUAL "undecided")
            set(result undecided)
        endif()
    endwhile()
    _aplomb_license_return(${result})
endfunction()

# Reads licences joined by AND.
function(_aplomb_license_and)
    _aplomb_license_term()
    set(result ${verdict})
    while(NOT malformed AND position LESS count)
        list(GET tokens ${position} token)
        if(NOT token STREQUAL "AND")
            break()
        endif()
        math(EXPR position "${position} + 1")
        _aplomb_license_term()
        if(result STREQUAL "refused" OR verdict STREQUAL "refused")
            set(result refused)
        elseif(result STREQUAL "undecided" OR verdict STREQUAL "undecided")
            set(result undecided)
        endif()
    endwhile()
    _aplomb_license_return(${result})
endfunction()

# Reads an expression in parentheses, or one licence with or without an exception.
function(_aplomb_license_term)
    _aplomb_license_next_token(token)
    if(malformed)
        _aplomb_license_return(undecided)
        return()
    endif()

    if(token STREQUAL "(")
        _aplomb_license_or()
        _aplomb_license_next_token(closing)
        if(NOT closing STREQUAL ")")
            set(malformed TRUE)
        endif()
        _aplomb_license_return(${verdict})
        return()
    endif()
    if(token MATCHES "^([()]|AND|OR|WITH)$")
        set(malformed TRUE)
        _aplomb_license_return(undecided)
        return()
    endif()

    set(license "${token}")
    set(exception "")
    if(position LESS count)
        list(GET tokens ${position} token)
        if(token STREQUAL "WITH")
            math(EXPR position "${position} + 1")
            _aplomb_license_next_token(exception)
            if(exception MATCHES "^([()]|AND|OR|WITH)$")
                set(malformed TRUE)
            endif()
        endif()
    endif()

    if(exception STREQUAL "")
        set(result refused)
        if(license IN_LIST APLOMB_ALLOWED_LICENSES)
            set(result allowed)
        endif()
    elseif("${license} WITH ${exception}" IN_LIST APLOMB_ALLOWED_LICENSES)
        set(result allowed)
    elseif(license IN_LIST APLOMB_ALLOWED_LICENSES)
        set(result undecided)
    else()
        set(result refused)
    endif()
    if(license MATCHES "^(NOASSERTION|NONE|LicenseRef-.*|DocumentRef-.*)$")
        set(result undecided)
    endif()
    _aplomb_license_return(${result})
endfunction()

# Sets out_token to the token at `position` and moves past it, or sets `malformed` at the end.
macro(_aplomb_license_next_token out_token)
    if(position LESS count)
        list(GET tokens ${position} ${out_token})
        math(EXPR position "${position} + 1")
    else()
        set(${out_token} "")
        set(malformed TRUE)
    endif()
endmacro()

# Hands a verdict, the position and whether the expression is malformed back to the caller.
macro(_aplomb_license_return verdict_value)
    set(verdict ${verdict_value} PARENT_SCOPE)
    set(position ${position} PARENT_SCOPE)
    set(malformed ${malformed} PARENT_SCOPE)
endmacro()

# Checks the licence of every port vcpkg installed, build-time tools and test-only dependencies
# included, against the allowlist. Configuring stops, naming every port the check refuses with its
# licence, and every port whose licence it can't decide that no one has reviewed.
#
#   MANIFEST_DIR   the directory of the project's vcpkg.json
#   INSTALLED_DIR  the tree `vcpkg install` left, holding every triplet
#   TRIPLET        the triplet the project builds for
#   REGISTRY       the git repository of vcpkg's built-in registry
#   NOTICES        the third-party notices file to write
function(aplomb_check_licenses)
    cmake_parse_arguments(PARSE_ARGV 0 arg "" "MANIFEST_DIR;INSTALLED_DIR;TRIPLET;REGISTRY;NOTICES" "")
    _aplomb_read_vcpkg_status("${arg_INSTALLED_DIR}")

    set(problems "")
    foreach(package IN LISTS packages)
        _aplomb_package_license("${arg_INSTALLED_DIR}" ${package} license)
        aplomb_license_verdict("${license}" verdict)
        _aplomb_split_package(${package} name triplet)
        if(verdict STREQUAL "refused")
            string(APPEND problems "\n  ${package} is under ${license}, which the allowlist refuses")
        elseif(verdict STREQUAL "undecided" AND NOT "${name}=${license}" IN_LIST APLOMB_REVIEWED_LICENSES)
            string(APPEND problems "\n  ${package} is under ${license}, which the check can't decide")
        endif()
    endforeach()

    if(problems)
        message(FATAL_ERROR
            "Dependencies' licences don't pass the allowlist in cmake/AplombLicenses.cmake. A licence "
            "the check can't decide needs a person to read it, and once it is found within the "
            "allowlist, to add it to APLOMB_REVIEWED_LICENSES there.${problems}"
        )
    endif()

    _aplomb_shipped_packages("${arg_MANIFEST_DIR}" "${arg_INSTALLED_DIR}" ${arg_TRIPLET}
        "${arg_REGISTRY}" shipped
    )
    _aplomb_write_notices("${arg_INSTALLED_DIR}" "${arg_NOTICES}" "${shipped}")
endfunction()

# Sets out_packages to the packages a host ships with aplomb, sorted: the project's own
# dependencies, and theirs in turn, but not the dependencies of the project's features, and not host
# dependencies, which only build the others. Reads what _aplomb_read_vcpkg_status sets.
#
# vcpkg's status lists a port's host dependencies among the others, so the port's own manifest tells
# them apart: an overlay port's from the project's overlays, and a registry port's from the git tree
# that vcpkg's SPDX document for it names.
function(_aplomb_shipped_packages manifest_dir installed_dir triplet registry out_packages)
    file(READ "${manifest_dir}/vcpkg.json" manifest)
    _aplomb_manifest_dependencies("${manifest}" dependencies host_dependencies)
    _aplomb_overlay_ports("${manifest_dir}" overlays)

    set(queue "")
    foreach(name IN LISTS dependencies)
        list(APPEND queue "${name}:${triplet}")
    endforeach()

    set(shipped "")
    while(queue)
        list(POP_FRONT queue package)
        # A dependency for another platform isn't installed at all.
        if(package IN_LIST shipped OR NOT package IN_LIST packages)
            continue()
        endif()
        list(APPEND shipped ${package})

        _aplomb_package_key(${package} key)
        if(NOT ${key}_depends)
            continue()
        endif()
        _aplomb_port_manifest("${installed_dir}" "${registry}" "${overlays}" ${package} port_manifest)
        _aplomb_manifest_host_tools("${port_manifest}" port_host_tools)
        _aplomb_split_package(${package} name package_triplet)
        foreach(dependency IN LISTS ${key}_depends)
            # vcpkg names a dependency by its port, with the features it needs in brackets, and with
            # its triplet only where that differs, which only a host dependency's can. Where the
            # triplets are the same, a port that is both a host tool and a library of this one is
            # listed once, and is shipped.
            string(REGEX MATCH "^([^:[]+)(\\[[^]]*\\])?(:.+)?$" matched "${dependency}")
            if(CMAKE_MATCH_3 OR CMAKE_MATCH_1 IN_LIST port_host_tools)
                continue()
            endif()
            list(APPEND queue "${CMAKE_MATCH_1}:${package_triplet}")
        endforeach()
    endwhile()
    list(SORT shipped)
    set(${out_packages} ${shipped} PARENT_SCOPE)
endfunction()

# Sets out_overlays to the overlay port directories in the vcpkg-configuration.json beside a
# project's manifest, as absolute paths.
function(_aplomb_overlay_ports manifest_dir out_overlays)
    set(overlays "")
    set(file "${manifest_dir}/vcpkg-configuration.json")
    if(EXISTS "${file}")
        file(READ "${file}" configuration)
        string(JSON count ERROR_VARIABLE error LENGTH "${configuration}" overlay-ports)
        if(NOT error AND count GREATER 0)
            math(EXPR last "${count} - 1")
            foreach(index RANGE ${last})
                string(JSON overlay GET "${configuration}" overlay-ports ${index})
                cmake_path(ABSOLUTE_PATH overlay BASE_DIRECTORY "${manifest_dir}" NORMALIZE)
                list(APPEND overlays "${overlay}")
            endforeach()
        endif()
    endif()
    set(${out_overlays} ${overlays} PARENT_SCOPE)
endfunction()

# Sets out_manifest to the manifest of the port that installed a package, as vcpkg built it.
function(_aplomb_port_manifest installed_dir registry overlays package out_manifest)
    _aplomb_split_package(${package} name triplet)
    _aplomb_package_spdx("${installed_dir}" ${package} spdx)
    set(location "")
    if(spdx)
        string(JSON location GET "${spdx}" downloadLocation)
    endif()

    if(location MATCHES "^git\\+.*@([0-9a-f]+)$")
        find_package(Git REQUIRED)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${registry}" cat-file blob "${CMAKE_MATCH_1}:vcpkg.json"
            OUTPUT_VARIABLE manifest
            RESULT_VARIABLE result
            ERROR_QUIET
        )
        if(result EQUAL 0)
            set(${out_manifest} "${manifest}" PARENT_SCOPE)
            return()
        endif()
    elseif(location STREQUAL "NOASSERTION")
        foreach(overlay IN LISTS overlays)
            if(EXISTS "${overlay}/${name}/vcpkg.json")
                file(READ "${overlay}/${name}/vcpkg.json" manifest)
                set(${out_manifest} "${manifest}" PARENT_SCOPE)
                return()
            endif()
        endforeach()
    endif()
    message(FATAL_ERROR
        "The licence check can't find the manifest of the port ${package}, so it can't tell which of "
        "its dependencies are host tools."
    )
endfunction()

# Sets out_host_tools to the names of the ports a vcpkg manifest, its features included, declares
# only as host dependencies, and never as ordinary ones.
function(_aplomb_manifest_host_tools manifest out_host_tools)
    _aplomb_manifest_dependencies("${manifest}" dependencies host_dependencies)
    string(JSON count ERROR_VARIABLE error LENGTH "${manifest}" features)
    if(NOT error AND count GREATER 0)
        math(EXPR last "${count} - 1")
        foreach(index RANGE ${last})
            string(JSON feature MEMBER "${manifest}" features ${index})
            string(JSON feature_manifest GET "${manifest}" features "${feature}")
            _aplomb_manifest_dependencies("${feature_manifest}"
                feature_dependencies feature_host_dependencies
            )
            list(APPEND dependencies ${feature_dependencies})
            list(APPEND host_dependencies ${feature_host_dependencies})
        endforeach()
    endif()
    if(dependencies)
        list(REMOVE_ITEM host_dependencies ${dependencies})
    endif()
    set(${out_host_tools} ${host_dependencies} PARENT_SCOPE)
endfunction()

# Sets out_dependencies to the names of the ordinary dependencies a vcpkg manifest declares, not its
# features', and out_host_dependencies to the names of its host dependencies.
function(_aplomb_manifest_dependencies manifest out_dependencies out_host_dependencies)
    set(dependencies "")
    set(host_dependencies "")
    string(JSON count ERROR_VARIABLE error LENGTH "${manifest}" dependencies)
    if(NOT error AND count GREATER 0)
        math(EXPR last "${count} - 1")
        foreach(index RANGE ${last})
            string(JSON type TYPE "${manifest}" dependencies ${index})
            if(type STREQUAL "STRING")
                string(JSON name GET "${manifest}" dependencies ${index})
                set(host OFF)
            else()
                string(JSON name GET "${manifest}" dependencies ${index} name)
                string(JSON host ERROR_VARIABLE error GET "${manifest}" dependencies ${index} host)
            endif()
            if(host)
                list(APPEND host_dependencies "${name}")
            else()
                list(APPEND dependencies "${name}")
            endif()
        endforeach()
    endif()
    set(${out_dependencies} ${dependencies} PARENT_SCOPE)
    set(${out_host_dependencies} ${host_dependencies} PARENT_SCOPE)
endfunction()

# Writes the third-party notices of the given packages: each one's version, licence, homepage and
# the licence text its port installs. Reads what _aplomb_read_vcpkg_status sets.
macro(_aplomb_write_notices installed_dir notices notices_packages)
    set(text [=[
# Third-party notices

aplomb is built on the libraries below, each under its own licence. A program that ships aplomb
carries the notices these licences ask for, which this file gathers.

This file is generated from vcpkg's metadata by the `third-party-notices` target. Don't edit it by
hand.
]=])
    foreach(package IN ITEMS ${notices_packages})
        _aplomb_split_package(${package} name triplet)
        _aplomb_package_key(${package} key)
        _aplomb_package_license("${installed_dir}" ${package} license)
        string(APPEND text "\n## ${name} ${${key}_version}\n\n- Licence: ${license}\n")
        _aplomb_package_spdx("${installed_dir}" ${package} spdx)
        set(homepage "")
        if(spdx)
            string(JSON homepage ERROR_VARIABLE error GET "${spdx}" homepage)
        endif()
        if(homepage AND NOT homepage MATCHES "^(null|.*-NOTFOUND)$")
            string(APPEND text "- Homepage: ${homepage}\n")
        endif()

        set(copyright "${installed_dir}/${triplet}/share/${name}/copyright")
        if(NOT EXISTS "${copyright}")
            message(FATAL_ERROR "The port ${package} installs no licence text for the notices.")
        endif()
        file(READ "${copyright}" copyright)
        string(STRIP "${copyright}" copyright)
        # A fence longer than any run of backticks in the text.
        set(fence "```")
        while(copyright MATCHES "${fence}")
            string(APPEND fence "`")
        endwhile()
        string(APPEND text "\n${fence}text\n${copyright}\n${fence}\n")
    endforeach()
    file(WRITE "${notices}" "${text}")
endmacro()

# Reads the packages from vcpkg's status database in an installed tree: its status file, then the
# files under updates, where vcpkg records what it installs and removes until it next folds them
# into the status. A paragraph takes the place of any earlier one for the same package and feature.
#
# Sets `packages` in the caller's scope to every installed port as <name>:<triplet>, sorted, and for
# each, through _aplomb_package_key, `<key>_version` to its version and `<key>_depends` to what its
# own paragraph and its installed features' paragraphs depend on, as vcpkg writes them.
macro(_aplomb_read_vcpkg_status installed_dir)
    file(GLOB updates "${installed_dir}/vcpkg/updates/*")
    list(SORT updates)
    set(paragraph_ids "")
    foreach(file IN ITEMS "${installed_dir}/vcpkg/status" ${updates})
        file(READ "${file}" status)
        # Each line becomes an element, and a blank line an empty element, which ends a paragraph.
        # Nothing the check reads holds a semicolon.
        string(REPLACE ";" "," status "${status}")
        string(REPLACE "\r" "" status "${status}")
        string(REPLACE "\n" ";" status "${status}")
        list(APPEND status "")

        set(fields "")
        foreach(line IN LISTS status)
            if(line MATCHES "^([A-Za-z-]+): (.*)$")
                string(MAKE_C_IDENTIFIER "${CMAKE_MATCH_1}" field)
                set(field_${field} "${CMAKE_MATCH_2}")
                list(APPEND fields ${field})
            elseif(line STREQUAL "" AND fields)
                string(MAKE_C_IDENTIFIER
                    "_aplomb_paragraph_${field_Package}:${field_Architecture}[${field_Feature}]" id
                )
                list(APPEND paragraph_ids ${id})
                set(${id}_package "${field_Package}:${field_Architecture}")
                set(${id}_version "${field_Version}")
                set(${id}_core TRUE)
                if(field_Feature)
                    set(${id}_core FALSE)
                endif()
                set(${id}_installed FALSE)
                if(field_Status MATCHES "install ok installed$")
                    set(${id}_installed TRUE)
                endif()
                string(REPLACE ", " ";" ${id}_depends "${field_Depends}")
                foreach(field IN LISTS fields)
                    unset(field_${field})
                endforeach()
                set(fields "")
            endif()
        endforeach()
    endforeach()

    list(REMOVE_DUPLICATES paragraph_ids)
    set(packages "")
    foreach(id IN LISTS paragraph_ids)
        if(${id}_installed)
            _aplomb_package_key(${${id}_package} key)
            list(APPEND ${key}_depends ${${id}_depends})
            if(${id}_core)
                list(APPEND packages ${${id}_package})
                set(${key}_version "${${id}_version}")
            endif()
        endif()
    endforeach()
    list(SORT packages)
endmacro()

# Sets out_name and out_triplet to the port and triplet of a package written <name>:<triplet>.
function(_aplomb_split_package package out_name out_triplet)
    string(REPLACE ":" ";" spec "${package}")
    list(GET spec 0 name)
    list(GET spec 1 triplet)
    set(${out_name} "${name}" PARENT_SCOPE)
    set(${out_triplet} "${triplet}" PARENT_SCOPE)
endfunction()

# Sets out_key to the name under which the check keeps what it knows of a package.
function(_aplomb_package_key package out_key)
    string(MAKE_C_IDENTIFIER "_aplomb_${package}" key)
    set(${out_key} ${key} PARENT_SCOPE)
endfunction()

# Sets out_license to the licence a package declares, as vcpkg's SPDX document for its port gives
# it: NOASSERTION where it declares none.
function(_aplomb_package_license installed_dir package out_license)
    _aplomb_package_spdx("${installed_dir}" ${package} spdx)
    set(license NOASSERTION)
    if(spdx)
        string(JSON declared ERROR_VARIABLE error GET "${spdx}" licenseConcluded)
        if(NOT error)
            set(license "${declared}")
        endif()
    endif()
    set(${out_license} "${license}" PARENT_SCOPE)
endfunction()

# Sets out_spdx to the package that describes a port in vcpkg's SPDX document for it, as JSON, or
# to nothing where there is no such document.
function(_aplomb_package_spdx installed_dir package out_spdx)
    _aplomb_split_package(${package} name triplet)
    set(file "${installed_dir}/${triplet}/share/${name}/vcpkg.spdx.json")
    set(result "")
    if(EXISTS "${file}")
        file(READ "${file}" document)
        string(JSON count LENGTH "${document}" packages)
        math(EXPR last "${count} - 1")
        foreach(index RANGE ${last})
            string(JSON id GET "${document}" packages ${index} SPDXID)
            if(id STREQUAL "SPDXRef-port")
                string(JSON result GET "${document}" packages ${index})
                break()
            endif()
        endforeach()
    endif()
    set(${out_spdx} "${result}" PARENT_SCOPE)
endfunction()
