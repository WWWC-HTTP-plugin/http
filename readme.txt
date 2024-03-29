WWWC Ver 1.0.4
------------------------

■ はじめに

WWWCはWindowsで動作するWebページの更新チェッカです。

よく見るWebページのURLを登録しておいて更新のチェックを行い、更新されたWebページ
を通知してくれるというものです。


配布ファイル
	・WWWC.exe		: WWWC本体
	・wwwc.dll		: HTTPのチェックを行うプロトコルDLL
	・wwwc.html		: マニュアル
	・ini_sample.txt	: INIファイル記述解説
	・filter_sample.txt	: フィルタ記述解説
	・readme.txt		: このファイル


■ 使い方

付属の wwwc.html を参照してください。


■ 更新履歴

・Ver 1.0.3 -> Ver 1.0.4
	・HTTPのサイズでのチェックで環境によって1バイトの誤差が出てしまうのを
	  修正した。これにより初回チェック時は更新アイテムが増える可能性があり
	  ます。
	・UPメッセージと検索からアイテムのプロパティを開きコメントに改行を含む
	  とアイテムが異常になってしまうのを修正した。
	・アイテムファイルの改行コードが正常ではない場合にアイテムが消えてしま
	  うのを回避するようにした。
	・検索画面にて、Ctrl+Tab で検索のタブを切り替え可能にした。
	・起動時にウィンドウの位置を補正しないようにした。
	・終了時にウィンドウの位置とサイズを強制保存するようにした。
	・クリップボードのチェーンから外れて貼り付けが無効の状態でもキー入力
	  (Ctrl+V)での貼り付けは可能になるようにした。
	・ヘッダ・ソース表示で選択文字が無い場合のコピーボタンは全文をコピーす
	  すようにした。
	・アイテムを開いた直後に終了するとプロセスが残る場合があったのを修正し
	  た。
	・アイテムやフィルタのファイルがロックされていても開けるようにした。
	・いくつかの重複するショートカットキーを修正した。

・Ver 1.0.2 -> Ver 1.0.3
	・アイテムに個別設定したProxyが有効になっていなかったのを修正した。
	・コメントで \ が含まれる2バイトコードをエスケープしてしまうのを修正し
	  た。
	・コメントの改行処理を改善した。
	・コマンドラインでアイテムを渡された場合にアイテムのフォーカスを追加ア
	  イテムの先頭になるようにした。
	・コマンドラインでフォルダが渡された場合に、リストが更新されないのを
	  修正した。
	・プラグインに渡すアイテムで * を指定した場合に登録されていないプロト
	  コルのアイテムが渡っていなかったのを修正した。
	・HTTPでIPアドレスのキャシュ処理を改善した。
	・Fileプロトコルのプロパティにコメント欄を追加した。
	・起動時チェック設定をオプション画面で設定できるようにした。
	・エクスプローラからフォルダを渡すときに送り先のサブフォルダを渡すと
	  エラーメッセージを表示するようにした。

・Ver 1.0.1 -> Ver 1.0.2
	・エラーアイテムのチェックを行うとソース表示やヘッダ表示が動作しないの
	  のを修正した。
	・iniファイルアクセスを改善した。
	・アイテムとフォルダのコメントで改行を保存できるようにした。
	・チェック中にアイテムのプロパティを開くとエラーになるのを修正した。
	・長すぎるURLの場合に正常にチェックできないのを修正した。
	・リダイレクト用のカウンタを初期化していないため転送エラーになってしま
	  うのを修正した。

・Ver 1.0.0 -> Ver 1.0.1
	・フォルダ設定(Folder.ini)の更新のタイミングを変更した。
	・フォルダ移動時にフォルダの更新マークが消えないように修正した。
	・HTTPのチェックでホスト名からIPアドレスに変換したものをキャッシュしない
	  オプションを加えた。(wwwc.iniの[HTTP]のHostNoCacheを1にする)
	・WM_NEXTCHECK によりプロトコルDLLから次のチェックへ移行できるようにし
	  た。
	・フィルター読み込みでメモリリークしていたのを修正した。
	・終了時イメージリスト解放でアクセス違反が起きる場合があったのを修正し
	  た。
	・検索時に不正メモリを参照する場合があったのを修正した。
	・ツリーとリストのフォントをiniファイルで変更できるようにした。
	・アイテム毎とフォルダ毎のチェックでアイテムが存在しない場合は前処理を行
	  わないようにした。
	・フィルタの条件に C - 大文字と小文字を区別しない を加えた。
	・アイテムのステータスが異常値だと保存に失敗するのを修正した。
	・ツリービューの背景色とテキスト色をiniファイルで設定できるようにした。
	・リストビューとツリービューのウィンドウスタイルをiniファイルで設定で
	  きるようにした。
	・フォルダ作成時に親フォルダのチェック情報を継承するようにした。
	  iniファイルで継承しないようにも設定可能。
	  [TREEVIEW] の SucceedFromParent
	・アイテム追加時に常にチェックしないアイテムとして追加できるオプションを
	  iniファイルに追加した。[ITEM] の DefNoCheck
	・プロトコル設定後httpアイテムでヘッダ表示とソース表示が正常に行えな
	  かったのを修正した。
	・ファイルのアイテムでオプションを設定できるようにした。
	・プラグイン用のメッセージの追加(アイテム実行やアイテム初期化)
	・DDEアプリケーション名に Netscape6とMozillaとTaBrowserを追加した。
	  (バージョンアップの場合は手動で追加 - ツールにWWWC.DLLから「ブラウザ情
	  報取得」を追加して「プロパティ」を押すと設定可能)
	・起動時にチェックする設定になっていて２重起動を許可しない場合に、既存の
	  ウィンドウでチェックを開始するようにした。
	・HTTPのリダイレクトで移動先のURLが完全ではない場合に正しくリダイレクト
	  されないのを修正した。
	・プロトコル設定のアイコン選択画面をシステムのものから自前のものに変更し
	  た。
	・フィルタの条件に R - このフィルタを繰り返し適用する を加えた。
	・iniファイルでListViewとTreeViewのアイコンのサイズを設定できるように
	  した。
	・Ctrl+Nで新規作成のメニューを表示するようにした。
	・HTTPのプロパティ(プロトコル)にDDEアプリケーションの設定タブを追加し
	  た。
	・ディスク容量のチェック方法を変更した。
	・フォルダ単位の自動チェック時にチェックの種類をプラグインに伝えていな
	  かったのを修正した。
	・ヘッダ、ソース表示が表示されない場合があったのを修正した。
	・更新アイテムを開いたときのアイコンの初期化方法を変更した。
	・インターネットショートカットをD&Dで追加するときにファイル名が長い場
	  合にエラーにならないように修正した。
	・オプション画面に「起動時にすべてのアイテムをチェック」のオプション
	  を加えた。


■ その他

旧WWWC(Ver 0.9.4)とアイテムの形式が違うためそのままではデータの持ち越しはでき
ません。
旧WWWC(Ver 0.9.4)からのアイテムのインポートは、しのぶさんが作ってくれています。
	しのぶさんのページ : http://www.shinobu-unet.ocn.ne.jp/~shinobu/
	インポートツール : http://www.nakka.com/wwwc/tool/wwwcimp2006.LZH

IEのお気に入りをD&DでWWWCに登録することも可能です。


WWWCはフリーソフトウェアです。配布、転載は自由です、特に許可を取る必要はありま
せん。

--

Special thanks:
  Jun-ya Kato
  Masaru Tsuchiyama
  Shinobu

--

/* Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
rights reserved.

License to copy and use this software is granted provided that it
is identified as the "RSA Data Security, Inc. MD5 Message-Digest
Algorithm" in all material mentioning or referencing this software
or this function.

License is also granted to make and use derivative works provided
that such works are identified as "derived from the RSA Data
Security, Inc. MD5 Message-Digest Algorithm" in all material
mentioning or referencing the derived work.

RSA Data Security, Inc. makes no representations concerning either
the merchantability of this software or the suitability of this
software for any particular purpose. It is provided "as is"
without express or implied warranty of any kind.

These notices must be retained in any copies of any part of this
documentation and/or software.
 */


------------------------

Copyright (C) 1996-2003 by Nakashima Tomoaki. All rights reserved.
	http://www.nakka.com/
	nakka@nakka.com

2003/03/12
