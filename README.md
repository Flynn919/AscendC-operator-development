# AscendC 算子开发学习资料

本仓库整理 Ascend C 自定义算子开发的学习笔记、示例工程和配套资料。

## 目录

- `学习总结（第一周）.md`：硬件架构、编程模型和流水线范式学习总结。
- `images/`：Markdown 笔记引用的配套图片，文档中的图片使用相对路径，可在 GitHub 页面直接显示。
- `AddKernelInvocationNeo/`：Add 算子示例工程，包含源码、CMake 配置、输入输出数据和验证脚本。
- `算子开发学习相关资料.docx`：配套 Word 学习资料。

## 示例工程

进入 `AddKernelInvocationNeo/` 后，可根据其中的 `README.md` 和 `使用说明.txt` 编译、运行和验证示例。构建产物、日志和临时文件已通过 `.gitignore` 排除，不提交到仓库。

## 说明

Markdown 文件与 `images/` 文件夹需要保持同级目录结构：

```text
学习总结（第一周）.md
images/
  p63_img0.png
  ...
```

