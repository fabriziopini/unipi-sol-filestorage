#!/bin/bash

valgrind --leak-check=full ./client                     \
-f ./mysock                                             \
-t 200                                                  \
-W ./OtherFiles/paperino.txt                            \
-a ./OtherFiles/paperino.txt,./OtherFiles/paperino.txt  \
-A ./Espulsi                                            \
-r ./OtherFiles/paperino.txt                            \
-d ./Letti                                              \
-w ./Files                                              \
-D ./Espulsi                                            \
-W ./OtherFiles/SubDir/topolino.txt                     \
-R                                                      \
-d ./Letti                                              \
-l ./OtherFiles/SubDir/topolino.txt                     \
-u ./OtherFiles/SubDir/topolino.txt                     \
-c ./OtherFiles/SubDir/topolino.txt                     \
-R                                                      \
-d ./LettiFinal                                         \
-p

echo "DONE"


if [ -e server.PID ]; then
    echo "Killing server..."

    kill -s SIGHUP $(cat server.PID)
    rm server.PID

    echo "Killed"
else
  echo "NO server pid found"
fi