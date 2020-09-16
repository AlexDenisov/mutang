set -e
set -v
set -x

env

export version=`grep -Eo "MULL_VERSION [0-9.]+" CMakeLists.txt | awk ' { print $2 } '`

case $TRAVIS_EVENT_TYPE in
  "pull_request")
    export suffix=-pr$TRAVIS_PULL_REQUEST
    export package=mull-nightly
    export channel=nightly
  ;;
  "push")
    if [ "$TRAVIS_TAG-" != "-" ];
    then
      export suffix=""
      export package=mull
      export channel=stable
      if [ $TRAVIS_TAG != $version ];
      then
        echo "Building only those tags that match the actual version"
        false
      fi
    else
      export suffix=-trunk`git rev-list HEAD --count`
      export package=mull-nightly
      export channel=nightly
    fi
  ;;
  *)
    false
  ;;
esac

pip install ansible
cd infrastructure && \
ansible-playbook macos-playbook.yaml \
  -e llvm_version="$LLVM_VERSION.0" \
  -e source_dir=$PWD/.. \
  -e gitref=$TRAVIS_COMMIT \
  -e host=localhost \
  -e SDKROOT=`xcrun -show-sdk-path` \
  -e mull_version=$version$suffix \
  --verbose
if [ $LLVM_VERSION == 9.0 ] && [ -n "$BINTRAY_API_KEY" ];
then
  curl --silent --show-error --location --request DELETE \
        --user "alexdenisov:$BINTRAY_API_KEY" \
        "https://api.bintray.com/packages/mull-project/macos/$package/versions/$version$suffix"
  curl --silent --show-error --fail --location --request PUT \
    --upload-file packages/`cat PACKAGE_FILE_NAME`.zip \
    --user "alexdenisov:$BINTRAY_API_KEY" \
    "https://api.bintray.com/content/mull-project/macos/$package/$version$suffix/`cat PACKAGE_FILE_NAME`.zip;publish=1"
fi
