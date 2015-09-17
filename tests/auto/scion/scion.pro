QT = core gui qml testlib scxml
CONFIG += testcase c++11 console
CONFIG -= app_bundle
TARGET = tst_scion

TEMPLATE = app

RESOURCES = $$OUT_PWD/scion.qrc

SOURCES += \
    tst_scion.cpp

defineReplace(nameTheNamespace) {
    sn=$$relative_path($$absolute_path($$dirname(1), $$OUT_PWD),$$SCXMLS_DIR)
    sn~=s/\\.txml$//
    sn~=s/[^a-zA-Z_0-9]/_/
    return ($$sn)
}
defineReplace(nameTheClass) {
    cn = $$basename(1)
    cn ~= s/\\.scxml$//
    cn ~=s/\\.txml$//
    cn ~= s/[^a-zA-Z_0-9]/_/
    return ($$cn)
}

win32 {
QSCXMLC_DEP=$$[QT_HOST_BINS]/qscxmlc.exe
QSCXMLC_CMD=$$QSCXMLC_DEP -no-c++11
} else {
QSCXMLC_DEP=$$[QT_HOST_BINS]/qscxmlc
QSCXMLC_CMD=$$QSCXMLC_DEP
}

myscxml.commands = $$QSCXMLC_CMD -oh scxml/${QMAKE_FUNC_nameTheNamespace}_${QMAKE_FILE_IN_BASE}.h -ocpp ${QMAKE_FILE_OUT} -namespace ${QMAKE_FUNC_nameTheNamespace} -classname ${QMAKE_FUNC_nameTheClass} ${QMAKE_FILE_IN}
myscxml.depends += $$QSCXMLC_DEP
myscxml.output = scxml/${QMAKE_FUNC_nameTheNamespace}_${QMAKE_FILE_IN_BASE}.cpp
myscxml.input = SCXMLS
myscxml.variable_out = SOURCES
QMAKE_EXTRA_COMPILERS += myscxml

myscxml_hdr.input = SCXMLS
myscxml_hdr.variable_out = SCXML_HEADERS
myscxml_hdr.commands = $$escape_expand(\\n)
myscxml_hdr.depends = scxml/${QMAKE_FUNC_nameTheNamespace}_${QMAKE_FILE_IN_BASE}.cpp
myscxml_hdr.output = scxml/${QMAKE_FUNC_nameTheNamespace}_${QMAKE_FILE_IN_BASE}.h
QMAKE_EXTRA_COMPILERS += myscxml_hdr

qtPrepareTool(QMAKE_MOC, moc)
#scxml_moc_source.CONFIG = no_link
scxml_moc_source.dependency_type = TYPE_C
scxml_moc_source.commands = $$QMAKE_MOC ${QMAKE_FILE_IN} -o ${QMAKE_FILE_OUT}
scxml_moc_source.output = scxml/$${QMAKE_H_MOD_MOC}${QMAKE_FILE_BASE}$${first(QMAKE_EXT_CPP)}
scxml_moc_source.input = SCXML_HEADERS
#scxml_moc_source.depends += $$WIN_INCLUDETEMP
scxml_moc_source.variable_out = SOURCES
silent:scxml_moc_source.commands = @echo moc ${QMAKE_FILE_IN} && $$scxml_moc_source.commands
QMAKE_EXTRA_COMPILERS += scxml_moc_source

SCXMLS_DIR += $$absolute_path($$PWD/../../3rdparty/scion-tests/scxml-test-framework/test)
ALLSCXMLS = $$files($$SCXMLS_DIR/*.scxml, true)

# For a better explanation about the "blacklisted" tests, see tst_scion.cpp
# <invoke>
BLACKLISTED = \
    test187.txml.scxml \
    test207.txml.scxml \
    test215.txml.scxml \
    test216.txml.scxml \
    test226.txml.scxml \
    test250.txml.scxml \
    test252.txml.scxml \
    test253.txml.scxml \
    test338.txml.scxml \
    test422.txml.scxml \
    test530.txml.scxml \
    test554.txml.scxml

# needs manual inspection
BLACKLISTED += \
    test230.txml.scxml \
    test307.txml.scxml \

# other
BLACKLISTED += \
    test239.txml.scxml \
    test242.txml.scxml \
    test276.txml.scxml \
    test301.txml.scxml \
    test441a.txml.scxml \
    test441b.txml.scxml \
    test552.txml.scxml \
    test557.txml.scxml \
    test558.txml.scxml

for (f,ALLSCXMLS) {
    cn = $$basename(f)
    if (!contains(BLACKLISTED, $$cn)) {
        SCXMLS += $$f

        cn ~= s/\\.scxml$//
        hn = $$cn
        cn ~=s/\\.txml$//
        sn = $$relative_path($$dirname(f), $$SCXMLS_DIR)
        sn ~=s/[^a-zA-Z_0-9]/_/

        inc_list += "$${LITERAL_HASH}include \"scxml/$${sn}_$${hn}.h\""
        func_list += "    []()->QScxmlStateMachine*{return new $${sn}::$${cn};},"

        base = $$relative_path($$f,$$absolute_path($$SCXMLS_DIR))
        tn = $$base
        tn ~= s/\\.scxml$//
        testBases += "    \"$$tn\","

        file = $$relative_path($$f, $$absolute_path($$PWD))
        qrc += '<file alias="$$base">$$file</file>'

        json = $$file
        json ~= s/\\.scxml$/.json/
        json_alias = $$base
        json_alias ~= s/\\.scxml$/.json/
        qrc += '<file alias="$$json_alias">$$json</file>'
    }
}

contents = $$inc_list "std::function<QScxmlStateMachine *()> creators[] = {" $$func_list "};"
write_file("scxml/compiled_tests.h", contents)|error("Aborting.")

contents = "const char *testBases[] = {" $$testBases "};"
write_file("scxml/scion.h", contents)|error("Aborting.")

contents = '<!DOCTYPE RCC><RCC version=\"1.0\">' '<qresource>' $$qrc '</qresource></RCC>'
write_file("scion.qrc", contents)|error("Aborting.")

load(qscxmlc)
