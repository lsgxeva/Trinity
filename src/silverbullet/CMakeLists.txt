FILE(GLOB_RECURSE SILVERBULLET_HEADER ${CMAKE_CURRENT_LIST_DIR}/include/silverbullet/*.h)
FILE(GLOB_RECURSE SILVERBULLET_SOURCE ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_LIST_DIR}/include)

CreateSourceGroups("${SILVERBULLET_SOURCE}" ${CMAKE_CURRENT_LIST_DIR})
CreateSourceGroups("${SILVERBULLET_HEADER}" ${CMAKE_CURRENT_LIST_DIR}/include/silverbullet)

ADD_LIBRARY(silverbullet OBJECT ${SILVERBULLET_SOURCE} ${SILVERBULLET_HEADER})