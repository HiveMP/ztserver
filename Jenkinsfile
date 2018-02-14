def gitCommit = ""
stage('Build') {
  def parallelMap = [:]
  parallelMap["Windows x64"] = {
    node('windows') {
      gitCommit = checkout(poll: true, changelog: true, scm: scm).GIT_COMMIT
      bat('git submodule update --init --recursive')
      bat('git clean -xdf build')
      bat('''
set PATH=%PATH:"=%
call "C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat"
cmake -H. -Bbuild -G "Visual Studio 15 2017 Win64"
''')
      bat('''
set PATH=%PATH:"=%
call "C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat"
cmake --build build --config Debug''')
      bat('''
set PATH=%PATH:"=%
call "C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat"
cmake --build build --config Release''')
      bat('''
set PATH=%PATH:"=%
call "C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat"
cmake --build build --config RelWithDebInfo''')
      bat('''
set PATH=%PATH:"=%
call "C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat"
cmake --build build --config MinSizeRel''')
      stash includes: 'build/Debug/**', name: 'win-x64-Debug'
      stash includes: 'build/Release/**', name: 'win-x64-Release'
      stash includes: 'build/RelWithDebInfo/**', name: 'win-x64-RelWithDebInfo'
      stash includes: 'build/MinSizeRel/**', name: 'win-x64-MinSizeRel'
    }
  }
  parallelMap["Windows x86"] = {
    node('windows') {
      checkout(poll: false, changelog: false, scm: scm)
      bat('git submodule update --init --recursive')
      bat('git clean -xdf build')
      bat('''
set PATH=%PATH:"=%
call "C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\VC\\Auxiliary\\Build\\vcvars32.bat"
cmake -H. -Bbuild -G "Visual Studio 15 2017"
''')
      bat('''
set PATH=%PATH:"=%
call "C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\VC\\Auxiliary\\Build\\vcvars32.bat"
cmake --build build --config Debug''')
      bat('''
set PATH=%PATH:"=%
call "C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\VC\\Auxiliary\\Build\\vcvars32.bat"
cmake --build build --config Release''')
      bat('''
set PATH=%PATH:"=%
call "C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\VC\\Auxiliary\\Build\\vcvars32.bat"
cmake --build build --config RelWithDebInfo''')
      bat('''
set PATH=%PATH:"=%
call "C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\VC\\Auxiliary\\Build\\vcvars32.bat"
cmake --build build --config MinSizeRel''')
      stash includes: 'build/Debug/**', name: 'win-x86-Debug'
      stash includes: 'build/Release/**', name: 'win-x86-Release'
      stash includes: 'build/RelWithDebInfo/**', name: 'win-x86-RelWithDebInfo'
      stash includes: 'build/MinSizeRel/**', name: 'win-x86-MinSizeRel'
    }
  }
  parallel (parallelMap)
}
stage('Archive') {
  node('linux') {
    def parallelMap = [:]
    ['x86', 'x64'].each { arch ->
      ['Debug', 'Release', 'MinSizeRel', 'RelWithDebInfo'].each { config ->
        parallelMap['win-' + arch + '-' + config] = {
          ws {
            sh('rm -Rf build || true')
            unstash ('win-' + arch + '-' + config)
            sh('zip -r win-' + arch + '-' + config + '-' + env.BUILD_NUMBER + '.zip build')
            stash includes: ('win-' + arch + '-' + config + '-' + env.BUILD_NUMBER + '.zip'), name: 'win-' + arch + '-' + config + '-archive'
          }
        }
      }
    }
    parallel (parallelMap)
  }
}
stage('Test') {
  node('windows') {
    checkout(poll: false, changelog: false, scm: scm)
    unstash ('win-x64-MinSizeRel')
    powerShell('Move-Item -Force build\\MinSizeRel\\*.dll test\\TestZt\\')
    powerShell('Move-Item -Force build\\MinSizeRel\\*.exe test\\TestZt\\')
    dir('test') {
      bat('''
set PATH=%PATH:"=%
call "C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Enterprise\\Common7\\Tools\\VsDevCmd.bat"
msbuild /m TestZt.sln''')
      dir('TestZt/bin/Debug/netcoreapp2.0') {
        bat('dotnet TestZt.dll')
      }
    }
  }
}
milestone label: 'Publish', ordinal: 20
stage('Publish to GitHub') {
  node('linux') {
    withCredentials([string(credentialsId: 'HiveMP-Deploy', variable: 'GITHUB_TOKEN')]) {
      timeout(3) {
        sh('\$GITHUB_RELEASE release --user HiveMP --repo ztserver --tag 0.' + env.BUILD_NUMBER + ' -c ' + gitCommit + ' -n "ztserver binaries (build #' + env.BUILD_NUMBER + ')" -d "This release is being created by the build server." -p')
        def parallelMap = [:]
        ['x86', 'x64'].each { arch ->
          ['Debug', 'Release', 'MinSizeRel', 'RelWithDebInfo'].each { config ->
            parallelMap['win-' + arch + '-' + config] = {
              ws {
                unstash 'win-' + arch + '-' + config + '-archive'
                sh('\$GITHUB_RELEASE upload --user HiveMP --repo ztserver --tag 0.' + env.BUILD_NUMBER + ' -n win-' + arch + '-' + config + '-' + env.BUILD_NUMBER + '.zip -f win-' + arch + '-' + config + '-' + env.BUILD_NUMBER + '.zip -l "ztserver for Windows (' + arch + ', ' + config + ')"')
              }
            }
          }
        }
        parallel (parallelMap)
        sh('\$GITHUB_RELEASE edit --user HiveMP --repo ztserver --tag 0.' + env.BUILD_NUMBER + ' -n "ztserver binaries (build #' + env.BUILD_NUMBER + ')" -d "These are automatically built binaries of ztserver. See the README for more information on usage."')
      }
    }
  }
}