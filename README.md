
这个项目有严重bug，对多文件补全，无法成功，原因是对sourcekit使用不当
参考 swift-master/tools/SourceKit/tools/complete-test/complete-test.cpp
compileArgs 多加了当前文件名  (commit 1ff0b96ca66a2070f4e45b35c2b0d5bf38f094f5)


猜测对于OC，Swift混编的项目，我们项目如此，不需要使用 (XcodeCompilationDatabase)对整个xcode输出编译log进行处理
因为编译swift只用了一条命令: swiftc
swiftc 带入了所有swift文件，所以命令很长，
每个文件都是使用同一条编译命令，所以传入sourcekit的compileArgs 都是一样的
在根目录下放置compile_commands.json，command可以从xcode中直接拷贝
[{
"command": "swiftc -module-name TestComplete -sdk (pathto)\/iPhoneOS.sdk -target arm64-apple-ios9.0 (pathtofile1)\/file1.swift (pathtofile2)\/file2.swift"
}]


修改YouCompleteMe/third_party/ycmd/cpp/ycm/CandidateRepository.cpp中一个变量，控制补全内容的最大长度，苹果API真长
    -const size_t MAX_CANDIDATE_SIZE = 80;
    +const size_t MAX_CANDIDATE_SIZE = 180;
重新编译YouCompleteMe:  sudo python install.py
icmFiles/swift 拷入YouCompleteMe/third_party/ycmd/ycmd/completers
icmFiles/icm_placeholder_behavior.vim  拷入YouCompleteMe/plugin  增加 tab 跳 <##>占位符功能

在.vimrc中加入ycm对swift的支持
    let g:ycm_semantic_triggers =  {
        \   'swift' : ['.', 're![_a-zA-Z]' ],
        \ }
    autocmd BufNewFile,BufRead *.swift set filetype=swift


本项目swiftyswiftvim  拷入 /YouCompleteMe/third_party/ycmd/third_party/swiftyswiftvim
编译后会产生build/http_server 等文件



# Swifty Swift Vim

Swifty Swift Vim is a semantic backend for the Swift programming language
tailored to the needs of editing source code.

The project was founded to fulfil the need of semantic Swift support in text editors and
integrate Swift into text editors.

![Travis](https://travis-ci.org/jerrymarino/swiftyswiftvim.svg?branch=master)

## iCompleteMe Vim Usage

[iCompleteMe](https://github.com/jerrymarino/icompleteme) implements the Vim
level UI code and client library.

![SwiftySwiftVimYCMPreview](https://cloud.githubusercontent.com/assets/1245820/26759463/4084bde8-48b3-11e7-869b-33ec00d70eef.gif)

Head on over to [iCompleteMe](https://github.com/jerrymarino/icompleteme) to
get up and running.

## emacs-ycmd

Support for icmd / SwiftySwiftVim has now been merged into [emacs-ycmd](https://github.com/abingham/emacs-ycmd). To get it working you can use the following:

```Emacs Lisp
(setq ycmd-server-command `(,(file-truename "~/.pyenv/shims/python")
                            ,(file-truename "~/.icmd/ycmd")))
(add-hook 'swift-mode-hook #'ycmd-mode)
(add-hook 'company-mode-hook #'company-ycmd-setup)
```

where `~/.icmd` points to your [icmd](https://github.com/jerrymarino/icmd) installation.

### Compilation Database

By default, it provides a basic level of completion support: completions within
a single file.

In most cases, build options and dependencies need to be specified to have a
good experience.

SwiftySwiftVim uses a [Compilation
Database](http://clang.llvm.org/docs/JSONCompilationDatabase.html) to import
compiler settings. Setup the build system to generate one at the workspace
root.

For Xcode *Project* users, [XcodeCompilationDatabase
](https://github.com/jerrymarino/XcodeCompilationDatabase) makes this easy.


## Supported Features

- Code Completion
- Semantic Diagnostics ( at the server level )

## Technical Design

It implements Semantic abilities based on the Swift programming language and
exposes them via an HTTP server.

It includes the following components

- A semantic engine for Swift.
- An HTTP server.

### Semantic Engine

The semantic engine is based on the Swift compiler and related APIs.

When reasonable, capabilities leverage SourceKit, the highest level Swift
tooling API. This leverages code reuse, and by using the highest level APIs as
possible, will simplify keeping the completer both feature complete and up to
date.

It makes calls to SourceKit via the `sourcekitd` client. This is to consumes
the SourceKit API at the highest level and is similar usage of these features in
Xcode.

It links against the prebuilt `sourcekitd` binary in Xcode to simplify
distribution and development.

### Features

It should support:
- code completion
- semantic diagnostics
- symbol navigation ( GoTo definition and more )
- symbol usage
- documentation rendering
- semantic searches

It should also support compile command configuration via flags and a JSON
compilation database to support complex projects, similar to clang's JSON
compilation database.  Finally, it should be blazing fast and stable.

### HTTP Frontend

Semantic abilities are exposed through an HTTP protocol. It should be high
performance and serve multiple requests at a time to meet the needs of users
who can make a lot of requests. The protocol is a JSON protocol to simplify
consumer usage and minimize dependencies.

The HTTP frontend is built on [Beast](https://github.com/vinniefalco/Beast)
HTTP and Boost ASIO, a platform for constructing high performance web services.

From a users perspective, logic runs out of the text editor's processes on the
HTTP server. The server is primarily designed to work with YCMD. See [Valloric's](https://val.markovic.io/articles/youcompleteme-as-a-server)
article for more on this.

## Development

`bootstrap` the repository and build.
```
  ./bootstrap
```

I log random musings about developing this and more in `notes.txt`.

This project is still in early phases, and development happens sporadically.

**Contributions welcome**

### Ideas for starter projects

- Improve readme and setup guide in iCompleteMe or iCMD
- Design an end to end integration testing system
- Get `GoToDefinition` working end to end
- Implement a semantic search engine

## Acknowledgements

The HTTP server stands on the shoulders of [Beast](https://github.com/vinniefalco/Beast)(s).
Many thanks to @vinniefalco for Beast and his guidance for getting this up and
running.

Thank you Apple for opening up the [Swift](https://github.com/apple/swift/) compiler and
IDE facilities. This project would not be possible without this.

Thanks to [@Valloric](https://github.com/Valloric/) and the [YCMD/YouCompleteMe](https://github.com/Valloric/YouCompleteMe/) team!

