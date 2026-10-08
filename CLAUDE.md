# CLAUDE.md

このワークスペースで作業するときの指針。2026-10-08 に `~/ros2_ws` 側のチャットで決めた内容をまとめたもの。同日、このワークスペースのチャットで決めたこと（ファイルの分け方・モデル・ビルド手順など）も追記した。

## 進め方（最優先）

**ユーザは勉強のために自分の手で作っている。Claude はやり方・考え方・注意点を教える役。**

- 頼まれない限り、コードやファイルを書かない・編集しない。「やり方だけ」「回答だけ」と言われたら、説明だけにとどめる。
- コード例は、要点を示す短い断片にとどめる。丸ごと実装したものは出さない。
- ドキュメント・コード内コメントは日本語。

## 何を作るか

RealSense で映像を取り、YOLO-pose で人の骨格を推定する。手を振っている人がいたら、二足ロボットの腕で手を振り返すデモ。

- 機体は `~/ros2_ws` と同じ二足ロボット（Raspberry Pi 5 / ROS 2 Jazzy）。
- 今回は腕だけ動かす。**後で脚（歩行）も動かす予定**なので、モータまわりは C++ で作る。

## ワークスペース構成

`~/ros2_ws` とは別のワークスペースで、ビルドはこの中だけで完結させる。

```
~/wave_demo_ws/src/
├── realsense-ros/      公式クローン（タグ 4.58.2）
├── wave_interfaces/    ament_cmake  自作 msg
├── wave_detection/     ament_python 検出のみ
│   └── wave_detector       画像 → 骨格推定 → WavingPeople
├── wave_motion/        ament_cmake  判断と動作生成（ノードは分ける）
│   ├── wave_responder      WavingPeople → "wave" 指令（判断）
│   └── motion_node         "wave" → 関節角の列 → feetech_driver でサーボへ。/joint_states を出す（後で歩行も）
└── feetech_driver/     ament_cmake  サーボドライバ（ROS 非依存のライブラリ。ノードはない）
    └── tools/              feetech_scan / feetech_jog（素の CLI）
```

ros2_ws の 3 層（`roboone_behavior` → `roboone_motion` → `feetech_servo`）と同じ分け方にしてある。feetech_driver は、ros2_ws の feetech_servo と同じく「ROS 非依存のライブラリ＋CLI ツール」で、motion_node が直接リンクして使う。

### 決めたことと理由

- **ワークスペースを分けた理由**: デモは独立したプログラムで、機体のリポジトリの履歴に混ぜたくないため。ros2_ws のパッケージは使わない（起動もしない）。
- **ros2_ws と同時に source しない**: 同じ名前のパッケージ（`realsense2_camera`）が二重に見えてしまう。wave_demo_ws は `/opt/ros/jazzy` の上だけでビルドする。
- **パッケージ名**:
  - msg は慣例どおり複数形の `*_interfaces`。
  - ドライバは汎用の部品なので、`wave_` を付けずにメーカー名を入れた `feetech_driver`。
- **判断を wave_motion に入れた理由**: 今は小さいため。ただし動作生成とはノードを分け、トピックでつなぐ。判断が増えたら（direction の方を向く、近づく など）、`wave_behavior` などに切り出す。
- **wave_responder と motion_node をトピックでつなぐ理由**: `ros2 topic pub` で "wave" を手で送れば、認識なしで腕の動きを試せる。
  - 指令は `std_msgs/String`（例 `/motion_command` に `"wave"`）。
- **feetech_driver をノードにしなかった理由**（2026-10-08 に変更。当初はノードにして JointState でつなぐ予定だった）:
  - 後で歩行を 200Hz で回すとき、トピックを経由しないので遅れも周期のばらつきも出ない。ros2_control も、ハードウェアはライブラリとして制御のプロセスに読み込む。
  - シリアルポートの持ち主が motion_node の 1 プロセスだけになる。
  - 代わりに motion_node が `/joint_states` を出す。サーボを直接試すときは、feetech_driver の CLI ツールを使う。
- **名前の変更**: ファイル名・ノード名・トピック名は後から変えられる。パッケージ名だけは変えるのが大変（package.xml、CMake / setup.py、依存側、古い build/install の削除）。
- **ファイル名の付け方**: ROS ノードのファイルは `*_node.py`（C++ は `*_node.cpp`）にする。ROS に依存しない中身は、`_node` を外した名前のファイルに置く（例 `wave_detector_node.py` と `wave_detector.py`、`motion_node.cpp` と `motion.cpp`）。
  - 「名前」は 3 種類あり、それぞれ別物。
    - ファイル名
    - 実行ファイル名（`setup.py` の `entry_points` の左側。`ros2 run` で打つ名前）
    - ノード名（`super().__init__()` に渡す名前。yaml の一番上のキーと合わせる）
  - ノードのクラス名は `*Node` にして、中身のクラスと区別する（例 `WaveDetectorNode` と `WaveDetector`）。
  - ビルドしたあとに `entry_points` を変えると、古い実行ファイルが `install/` に残る。`build/<pkg>` と `install/<pkg>` を消してからビルドし直す。

## 骨格推定: YOLO-pose（Python）

- **OpenPose はやめた**: CUDA 前提で、Pi 5 の CPU では 1 フレームに数秒かかるため。
- **MediaPipe Pose もやめた**: 旧 API は 1 人しか検出せず、複数人の認識に向かないため。
- **人の ID**: ultralytics の `model.track(..., persist=True)`。トラッカー（ByteTrack / BoT-SORT）がフレームをまたいで ID を付ける。
- **Python にした理由**: 時間のほとんどは推論で、推論はネイティブで動くため、C++ にしてもほぼ速くならない。速さに効くのは次の 2 つ。
  - NCNN 形式への書き出し（`yolo export model=yolo26n-pose.pt format=ncnn imgsz=320`）
  - `imgsz` を小さくすること
- **モデル**: `yolo26n-pose` を使う（当初は `yolo11n-pose` の予定だった）。
  - NCNN に書き出したものが `~/wave_demo_ws/models/yolo26n-pose_ncnn_model/` にある。
  - 推論時の imgsz は、書き出したときと同じ 320 にする。
  - 返ってくるキーポイントの座標は、元の画像（640x480）の画素。なので camera_info の fx, cx をそのまま使える。
- **fps の目標は 8〜10fps 以上**: 手を振る速さは 1.5〜3Hz。周波数を測るには、その 2 倍を超える fps が要る（足りないと実際より遅い周波数に見える）。
- **作り方**:
  - 推定が遅いので、古いフレームを溜めない。キュー深さは 1 にし、最新の 1 枚だけをタイマーで処理する。
  - 画像の購読は BEST_EFFORT・深さ 1。`qos_profile_sensor_data` は深さが 5 なので、そのままでは使わない。
  - ファイルは、ROS ノード（`wave_detector_node.py`）と認識の中身（`wave_detector.py`）の 2 つに分ける。
    - 当初は中身を `pose_estimator.py` と `wave_logic.py` に分ける予定だった。量が少ないので 1 ファイルにまとめた。
    - `wave_detector.py` では次を守る。
      - rclpy・sensor_msgs・cv_bridge を import しない。入力は BGR の numpy 配列と時刻 [s]、出力は dataclass のリスト。
      - 骨格推定と手を振る判定は、クラスを分ける（例 `PoseEstimator` と `WaveJudge`）。判定のテストは、手作りの人のデータを渡して pytest で行う。
      - ultralytics は、骨格推定のクラスの `__init__` の中で import する。判定のテストで torch を読み込まないようにするため。
      - 時刻はノードから渡す（画像の `header.stamp` を秒にしたもの）。中で `time.time()` を呼ばない。
      - 長くなったら（200〜300 行が目安）、判定を `wave_logic.py` に切り出す。
  - 骨格を描き込んだデバッグ画像（例 `/wave_detection/debug_image`）を出し、`rqt_image_view` で見る。
    - publish は RELIABLE（既定）にする。BEST_EFFORT で出すと、RELIABLE で購読する道具には届かない。
    - 購読している人がいないとき（`get_subscription_count() == 0`）は、描画と変換を省く。
  - しきい値（往復回数・判定する時間の長さ）や、振り返したあとのクールダウン秒数は yaml に出す。
- **ultralytics のインストールで詰まりやすいところ**:
  - Ubuntu 24.04 ではシステムの Python に pip で入れられない。venv を `--system-site-packages` 付きで作る（rclpy を見えたままにするため）。venv のディレクトリには `COLCON_IGNORE` を置く。
  - numpy 2.x が入ると、ROS Jazzy の cv_bridge が壊れる。`numpy<2` で固定する。
  - 固定は venv の `pip.conf`（`[install] constraint = ~/wave_demo_ws/venv/constraints.txt`）で行っている。中身は `numpy<2` と `setuptools<80`。ultralytics が初回に自分で pip install するときも、この制約が効く。
  - PyTorch で 1〜2GB 使う（ディスクの空きは約 35GB）。
- **venv の ultralytics を使うパッケージは、venv を有効にして `python -m colcon build` でビルドする。**
  - 普通の `colcon build` だと、実行ファイルの shebang が `/usr/bin/python3` になる。すると `ros2 run` で ultralytics が見つからない。
  - 確認は `head -1 install/<pkg>/lib/<pkg>/<実行ファイル>`。venv の python になっていれば正しい。
- venv の ultralytics は rosdep のキーではないので、package.xml には書かない。
- モデルファイルは git に入れない。パスは yaml で渡す。
- ament_python では、`launch/` と `config/` を `setup.py` の `data_files` に書かないとインストールされない。

## 自作 msg（wave_interfaces）

`WavingPerson.msg`
```
int32 id               # トラッキング ID
float32 direction      # 水平方向 [rad]。カメラ正面 0、左が +（REP 103）
float32 wave_freq      # 手を振る周波数 [Hz]。振っていなければ 0
bool is_waving
```

`WavingPeople.msg`
```
std_msgs/Header header # stamp は元の画像の stamp
WavingPerson[] people  # そのフレームの全員。誰もいなければ空
```

- direction は、キーポイントの画素位置 u と `/camera/color/camera_info` の fx, cx から `atan((cx - u) / fx)` で求める。
- CMake は `rosidl_generate_interfaces(... DEPENDENCIES std_msgs)`。package.xml には次を書く。
  - `rosidl_default_generators`（buildtool_depend）
  - `rosidl_default_runtime`（exec_depend）
  - `<member_of_group>rosidl_interface_packages</member_of_group>`

## RealSense

ros2_ws と同じく、**apt ではなくソースからビルドしたもの**を使う。Pi のカーネルには UVC のメタデータパッチが当てられないため、RSUSB バックエンドでビルドしてある。

- **SDK（librealsense2 v2.58.2）**: `/usr/local` にシステム全体として入っている。入れ直す必要はない。
- **ROS ラッパー（realsense-ros）**: このワークスペースにクローンする。**タグは SDK に合わせて必ず 4.58.2 にする。**
  ```bash
  cd ~/wave_demo_ws/src
  git clone -b 4.58.2 --depth 1 https://github.com/IntelRealSense/realsense-ros.git
  ```
- **rosdep では `--skip-keys=librealsense2` を付ける**。`realsense2_camera` の package.xml に `librealsense2` があるので、付けないと apt 版の SDK が入り、`/usr/local` のものとぶつかる。
  ```bash
  rosdep install --from-paths src --ignore-src -r -y --skip-keys=librealsense2
  ```
- ros2_ws の `realsense_bringup` は自作のラッパーで、このワークスペースにはない。launch は自分で書く。
  - 例: `ros2 launch realsense2_camera rs_launch.py camera_namespace:='' enable_color:=true enable_depth:=false rgb_camera.color_profile:=640x480x30`
  - 4.58.2 の `rs_launch.py` は、名前空間と名前の既定がどちらも `camera`。そのままだとトピックが `/camera/camera/...` になる。`camera_namespace:=''` を付けると `/camera/color/...` になる。
  - 今は色の画像しか使わないので、`enable_depth:=false` にして USB と CPU を空ける。
  - 解像度は 640x480 か 848x480 程度にする。この Pi の USB では 1920x1080 のカラーで帯域が足りなくなる。
  - 設定値は `~/ros2_ws/src/realsense_bringup/launch/realsense.launch.py` が参考になる。
- カメラを開けるプロセスは 1 つだけ。

## サーボドライバ（feetech_driver, C++）

- **SDK はパッケージに同梱する**: `~/ros2_ws/src/feetech_servo/vendor/scservo` をコピーする（MIT ライセンス。LICENSE は残す）。小さくてソースしか配布されていないので、同梱して静的リンクする。版がコードと一緒に固定される利点もある。CMake は `~/ros2_ws/src/feetech_servo/CMakeLists.txt` の前半が参考になる。
- **ROS 非依存のライブラリにする**（ノードは作らない。rclcpp に依存しない）。motion_node がリンクして使う。
- **構成**:
  ```
  feetech_driver/
  ├── vendor/scservo/              公式 SDK（書き換えない）
  ├── include/feetech_driver/      公開ヘッダ。SDK のヘッダは include しない
  │   ├── servo_bus.hpp            1 バス = 1 オブジェクト。「ID とカウント」で話す
  │   ├── joint_config.hpp         関節の設定・rad ↔ カウントの変換・yaml の読み込み
  │   └── joint_driver.hpp         複数のバスをまとめ、「関節名と rad」で話す（motion_node が使う）
  ├── src/
  ├── tools/                       素の CLI（rclcpp なし）。手で使う道具は test/ ではなくここ
  │   ├── feetech_scan.cpp         読み取り専用。ping と現在位置
  │   └── feetech_jog.cpp          1 軸だけ動かす（--allow-torque のときだけトルクを入れる）
  └── test/test_joint_config.cpp   変換の gtest（実機なしで回る）
  ```
  - クラス名は例。
  - `ros2 pkg create feetech_driver --build-type ament_cmake --license MIT --library-name feetech_driver` の雛形から作る。
- **設計**:
  - `ServoBus`: ポート 1 本と `HLSCL` 1 個を持つ。サーボの癖（Torque 引数・速度を 0 にしない、EEPROM の関数を持たない）への対処は、ここにまとめる。ポートが開けなければ例外を投げ、dialout と `/dev/feetech_*` を確かめるようメッセージに書く。
  - `JointConfig`: カウント（4096/回転）・原点のずれ・向きの反転への変換と、可動域でのクランプを、ここ 1 か所だけで行う。純粋な計算なので、gtest で最初にテストする。
  - `JointDriver`: 外とのやり取りは「関節名と rad」で行う。関節の順番は作ったときに決め、以後は `std::vector<double>` でやり取りする。
    - 読み取りに失敗した関節は、前回の値を保持し、失敗の回数を数える。
    - トルクを入れるときは、現在位置を読む → 目標を現在位置にする → トルクを入れる、の順。
    - デストラクタでトルクを切る。
    - 脚まで入れて 200Hz で回すときは、バスごとのスレッドをこの中に作る（使い方は変えない）。
  - 書き込みは `SyncWritePosEx` で、バスごとに全軸を 1 パケットにまとめる。
  - 読み取りは、最初は 1 軸ずつの往復でよい。軸が増えて周期が足りなくなったら SyncRead に替える。
    - `HLSCL` は `SCS` を継承していて、`syncReadPacketTx/Rx` が使える。
    - ros2_ws の `feetech_bus.cpp` の `sync_read_states` が参考になる。ただし ros2_ws のこのファイルは `SMS_STS` を使い、レジスタのブロックを自分で組み立てて書き込んでいる。こちらでは `HLSCL` を使う。
  - 関節名 ↔ バス・ID・原点・向き・可動域の対応は yaml に書く。**この機体用の情報なので、feetech_driver ではなく使う側（`wave_motion/config/servos.yaml`）に置く。**
- **SDK を外に見せない**:
  - 公開ヘッダでは `class HLSCL;` と前方宣言し、`std::unique_ptr<HLSCL>` で持つ。
  - デストラクタは `.cpp` に書く（ヘッダに書くと incomplete type のエラーになる）。
  - CMake:
    - SDK は `scservo` という STATIC のライブラリにする。`POSITION_INDEPENDENT_CODE ON` と `-w` を付ける。
    - 自分のライブラリは `add_library(feetech_driver SHARED ...)` にし、`target_link_libraries(feetech_driver PRIVATE scservo yaml-cpp::yaml-cpp)` でリンクする。
    - SHARED のライブラリに PRIVATE でリンクしているので、scservo は export に含めなくてよい（CMake で確認済み）。
    - export は `ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)`。使う側は `feetech_driver::feetech_driver` をリンクする。
- **作る順番**:
  1. SDK のコピーと CMake。空に近いライブラリがビルドできるところまで
  2. 変換の gtest
  3. scan（読み取り専用。ping と現在位置の表示）
  4. 1 軸だけ動かすツール（`--allow-torque` を付けたときだけトルクを入れる）
  5. motion_node から使う（腕 6 軸で 50Hz 程度から）。最初は読み取りと `/joint_states` を出すことだけにする
- **ros2_control** という本格的な道もあるが、最初は自作の仕組みで理解してから。

## 動作生成（wave_motion, C++）

- ノードは `wave_responder_node.cpp` と `motion_node.cpp` の 2 つ。動作の生成（時刻 → 関節角）は ROS 非依存の `motion.hpp/.cpp` に分ける。
- `ros2 pkg create wave_motion --build-type ament_cmake --license MIT --dependencies rclcpp std_msgs sensor_msgs wave_interfaces` で作った。
  - `feetech_driver` への依存（`find_package` と package.xml）は、ライブラリがビルドできるようになってから足す。
- motion_node のパラメータ:
  - `allow_torque`: **既定は false**。false のあいだは、サーボを読んで `/joint_states` を出すだけにする。トルクも指令も出さない。
  - `servo_config`: `servos.yaml` のパス。
- `JointDriver` は motion_node のメンバ（`std::unique_ptr`）にする。Ctrl-C で `spin` が戻ってノードが破棄されるとき、デストラクタでトルクが切れる。
  - 異常終了（segfault など）ではデストラクタが呼ばれない。最後の目標位置でトルクが入ったまま止まる。

### 実機の構成

- **バスは 2 本**。udev の固定名 `/dev/feetech_left` と `/dev/feetech_right` を使う（ttyACM の番号は実際に入れ替わったことがある）。1Mbps。
- 左半身は ID 1-6, 8-10、右半身は ID 1-10。**ID7 があるのは右だけ**で、腕ではなく腰のヨー（カメラも乗っている）。
- 脚は ID1 股ピッチ / ID2 股ロール / ID3 股ヨー / ID4 膝（4 節リンク）/ ID5・ID6 足首（パラレルリンク）。
- 腕は左右とも ID8〜10。T ポーズを基準に、根元の ID8 がピッチ、ID9 がロール、先の ID10 がヨー。
- 全軸 HLS 系（型番 4618 / 5130）。

### サーボの癖（実機で踏んだもの）

- SDK は **`HLSCL` クラス**を使う（`SMS_STS` ではない）。
- **`WritePosEx` などの `Torque` 引数は既定が 0。0 だとまったく動かない**（reg44/45 = 目標トルク）。ros2_ws では 2047 を渡している。
- **速度に 0 を渡すと、トルクは入るが動かない**。0 は無制限ではない。
- 電圧が低いと、通信がときどき抜ける。読み取りが失敗してもノードが落ちないようにする（前回の値を保持する、失敗回数を数える）。
- EEPROM（ID・角度リミットなど）には書き込まない（`unLockEprom` を呼ばない）。
- シリアルポートを使うには、ユーザが `dialout` グループに入っている必要がある。
- シリアルポートを開けるプロセスは 1 つだけ。ros2_ws の motion_node などと同時に動かさない。

## 安全の作法

- **既定は読み取り専用**。トルクを入れること・位置指令を送ることは、明示的なフラグ（`allow_torque` の既定を false にするなど）を付けたときだけにする。
- トルクを入れる前に現在位置を読み、目標位置を現在位置にしてから入れる（そうしないと起動時に腕が跳ねる）。
- 指令は yaml の可動域でクランプする。終了時（Ctrl-C を含む）はトルクを切る。
- 指令が途絶えたら、その場で保持する。feetech_driver は motion_node と同じプロセスで動くので、書き込みが止まれば、サーボは最後の目標で止まる。
- 初めて動かすときは腕が何にも当たらない姿勢にし、速度は遅くから始める。
- Claude がトルクを入れるコマンドを提案するときは、毎回そう明言する。

## 参考: ros2_ws 側のメモリ

機体の実機の癖は `~/.claude/projects/-home-auto-ros2-ws/memory/` にある（索引は `MEMORY.md`）。
必要なときだけ読む。**書き込まない**（このワークスペースで覚えたことは、こちらのメモリに書く）。

- 大半は歩行・IMU・相手検知の調整の記録で、このデモには関係しない。
- サーボまわり（`feetech_*.md`、`arm_joint_axes.md`）は、要点を上の「サーボドライバ」節に書き写してある。
- 脚を動かす段階になったら、脚の軸・逆運動学・サーボの遅れなどの項目が参考になる。
- 中身は書かれた時点のもの。ファイル名や設定値を挙げるときは、今も合っているか確かめる。

## その他

- `git init` 済み。`.gitignore` に入れているもの:
  - `build/` `install/` `log/`
  - `venv/`
  - `models/` `runs/`（モデルと、推論結果の出力先）
  - `src/realsense-ros/`。公式のクローンで、入れ子のリポジトリになるため除外した。版はタグ 4.58.2 で固定している。
- 名前を変えるときは `git mv` を使う（git がまだ管理していないファイルは普通の `mv`）。
- Pi には SSH で入っていて、画面がない。`rqt_image_view` などの GUI は、次のどちらかで動かす。
  - Windows の VcXsrv に転送する。**`ssh -Y` を使う（`-X` では転送されない）。** 手順はメモリ `x11-from-windows.md` にある。
  - 同じ LAN・同じ `ROS_DOMAIN_ID` の PC 側で実行する
