# Native editing session extracted from Reader. No GUI or language backend initialization.
if(NOT TARGET CyCode::Files)
  message(FATAL_ERROR "Add CyCode/common before Documents")
endif()
find_package(Threads REQUIRED)
add_library(cycode_documents STATIC EXCLUDE_FROM_ALL
  src/Document.cpp src/Encoding.cpp src/Search.cpp src/TextPlugins.cpp src/DocumentSession.cpp)
target_include_directories(cycode_documents PUBLIC include)
target_link_libraries(cycode_documents PUBLIC CyCode::Files Threads::Threads ${CMAKE_DL_LIBS})
set_target_properties(cycode_documents PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(CyCode::Documents ALIAS cycode_documents)
add_library(CyCode::TextSession ALIAS cycode_documents)
add_library(cycode_text_contracts INTERFACE)
target_include_directories(cycode_text_contracts INTERFACE include)
target_link_libraries(cycode_text_contracts INTERFACE CyCode::Files)
add_library(CyCode::TextContracts ALIAS cycode_text_contracts)

target_sources(cycode_documents PRIVATE src/DocumentState.cpp)
