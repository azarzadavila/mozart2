#!Groovy

pipeline {

    agent any

    stages {

        stage ('Generate sources') {
            options { 
                checkoutToSubdirectory('mozart2')
            }
            steps {
                dir ('llvm') {
                    sh 'curl http://releases.llvm.org/4.0.1/llvm-4.0.1.src.tar.xz | tar xJf - --strip-components=1'
                    
                    dir ('tools/clang') {
                        sh 'curl http://releases.llvm.org/4.0.1/cfe-4.0.1.src.tar.xz | tar xJf - --strip-components=1'
                        sh 'export PREFIX=$(pwd)'
                    }
                }
                dir ('llvm-build') {
                    sh 'cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX:PATH=`pwd`/../llvm-install ../llvm'
                    sh 'make -j4'
                    sh 'make install -j4'
                }
                dir ('llvm-install') {
                    sh 'export PATH=`pwd`/bin:$PATH'
                }
                checkout scm
                dir('build') {
                    sh 'cmake -DCMAKE_BUILD_TYPE=Release -DMOZART_BOOST_USE_STATIC_LIBS=False -DCMAKE_PROGRAM_PATH=$BIN -DCMAKE_PREFIX_PATH=$PREFIX $SOURCES'
                    sh 'make dist VERBOSE=1 -j4'
                }
            }
        }

        stage ('Build') {
            steps {
                echo 'lol'
            }
        }

    }
}
