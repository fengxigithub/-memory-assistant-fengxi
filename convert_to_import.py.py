#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
知识点批量转换工具
功能：将 CSV 或 TXT 文件转换为符合“知识点记忆系统”导入格式的 JSON 文件。

用法：
  1. 双击本脚本：自动处理当前文件夹下所有 .csv 和 .txt 文件，
     生成对应的 文件名_import.json。
  2. 命令行指定文件：
     python convert_to_import.py 文件.csv
     python convert_to_import.py 文件.txt
     可同时指定多个文件，用空格分隔。

CSV 格式要求（需包含列名）：
  标题,内容,分类,图片路径
  什么是艾宾浩斯？,德国心理学家...,心理学,
  Qt信号与槽,Qt核心机制...,编程,

TXT 格式要求（默认每行一个知识点，标题和内容用制表符分隔）：
  什么是Python？\t一种解释型语言。
  变量命名规则\t不能以数字开头...
"""

import csv
import json
import sys
import os


import csv
import json
import os

def smart_split_paths(raw):
    """智能分割图片路径字符串，返回路径列表"""
    if not raw:
        return []
    raw = raw.strip()
    # 去除可能包裹整个字符串的引号（比如 "path1.png;path2.png"）
    if (raw.startswith('"') and raw.endswith('"')) or (raw.startswith("'") and raw.endswith("'")):
        raw = raw[1:-1].strip()

    # 候选分隔符，按优先级尝试（分号、逗号、竖线、换行、制表符）
    separators = [';', ',', '|', '\n', '\t']
    best_sep = None
    best_count = 0
    for sep in separators:
        cnt = raw.count(sep)
        if cnt > best_count:
            best_count = cnt
            best_sep = sep
    if best_count == 0:
        # 没有分隔符，直接返回清理后的单个路径
        return [raw.strip('"\'')]

    # 按最佳分隔符分割，并清理每个片段
    parts = raw.split(best_sep)
    paths = []
    for part in parts:
        p = part.strip().strip('"\'')
        if p:
            paths.append(p)
    return paths

def csv_to_json(csv_path, output_path=None):
    if output_path is None:
        base = os.path.splitext(os.path.basename(csv_path))[0]
        output_path = base + "_import.json"

    items = []
    # 尝试多种编码
    encodings = ['utf-8-sig', 'utf-8', 'gbk', 'gb2312', 'ansi']
    data = None
    used_encoding = None
    for enc in encodings:
        try:
            with open(csv_path, 'r', encoding=enc) as f:
                data = f.read()
            used_encoding = enc
            break
        except UnicodeDecodeError:
            continue
    if data is None:
        print(f"❌ 无法以任何已知编码读取文件：{csv_path}")
        return False

    try:
        reader = csv.DictReader(data.splitlines())
        for row in reader:
            title = row.get('标题', '').strip()
            if not title:
                continue

            # 智能分割图片路径（支持 ; , | 等分隔符）
            raw_paths = row.get('图片路径', '').strip()
            paths = smart_split_paths(raw_paths) if raw_paths else []

            item = {
                "title": title,
                "content": row.get('内容', '').strip(),
                "category": row.get('分类', '未分类').strip(),
                "imagePaths": paths   # 输出为数组
            }
            items.append(item)
    except Exception as e:
        print(f"❌ 解析 CSV 内容失败：{csv_path}\n{str(e)}")
        return False

    # 保存为 JSON 文件
    with open(output_path, 'w', encoding='utf-8') as f:
        json.dump(items, f, ensure_ascii=False, indent=2)
    print(f"✅ 转换成功：{csv_path} → {output_path} （{len(items)} 条）")
    return True


def txt_to_json(txt_path, delimiter='\t', output_path=None):
    """将 TXT 文件转换为 JSON 知识点数组"""
    if output_path is None:
        base = os.path.splitext(os.path.basename(txt_path))[0]
        output_path = base + "_import.json"

    items = []
    try:
        with open(txt_path, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                parts = line.split(delimiter, 1)
                title = parts[0].strip()
                content = parts[1].strip() if len(parts) > 1 else ""
                items.append({
                    "title": title,
                    "content": content,
                    "category": "未分类",
                    "imagePath": ""
                })
    except Exception as e:
        print(f"❌ 读取 TXT 文件失败：{txt_path}\n{str(e)}")
        return False

    if not items:
        print(f"⚠️  未从 {txt_path} 中提取到有效数据。")
        return False

    with open(output_path, 'w', encoding='utf-8') as f:
        json.dump(items, f, ensure_ascii=False, indent=2)
    print(f"✅ 转换成功：{txt_path} → {output_path} （{len(items)} 条）")
    return True

def process_file(filepath):
    """根据扩展名自动调用对应的转换函数"""
    ext = os.path.splitext(filepath)[1].lower()
    if ext == '.csv':
        return csv_to_json(filepath)
    elif ext == '.txt':
        return txt_to_json(filepath)
    else:
        print(f"⏭️  跳过不支持的文件类型：{filepath}")
        return False

def main():
    if len(sys.argv) >= 2:
        # 命令行模式：处理指定文件
        files = sys.argv[1:]
        for f in files:
            if os.path.isfile(f):
                process_file(f)
            else:
                print(f"❌ 文件不存在：{f}")
    else:
        # 无参数模式：处理当前目录下所有 CSV/TXT
        current_dir = os.getcwd()
        print(f"🔍 正在扫描文件夹：{current_dir}")
        csv_files = [f for f in os.listdir('.') if f.lower().endswith('.csv')]
        txt_files = [f for f in os.listdir('.') if f.lower().endswith('.txt')]
        all_files = csv_files + txt_files
        if not all_files:
            print("❌ 当前文件夹没有找到 .csv 或 .txt 文件。")
            print("请将本脚本复制到包含题库文件的文件夹后再运行。")
            input("按回车键退出...")
            return
        for f in all_files:
            process_file(f)

    # 如果是在 Windows 下直接双击运行，防止窗口一闪而过
    if sys.platform.startswith('win') and len(sys.argv) < 2:
        input("\n转换完成，按回车键退出...")

if __name__ == "__main__":
    main()
