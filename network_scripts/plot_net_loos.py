#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import re
import pandas as pd
import matplotlib.pyplot as plt
import sys
from pathlib import Path

def parse_log_line(line):
    """
    解析一行日志，格式例如：
    2025-05-21 15:04:05 eth0 RX_loss_rate=0.000120 TX_loss_rate=0.000030
    返回字典：{'timestamp': datetime, 'iface': str, 'rx': float, 'tx': float}
    """
    pattern = (
        r'^(?P<ts>\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})\s+'
        r'(?P<iface>\S+)\s+RX_loss_rate=(?P<rx>[0-9.]+)\s+TX_loss_rate=(?P<tx>[0-9.]+)'
    )
    m = re.match(pattern, line.strip())
    if not m:
        return None
    d = m.groupdict()
    return {
        'timestamp': pd.to_datetime(d['ts']),
        'iface': d['iface'],
        'rx': float(d['rx']),
        'tx': float(d['tx']),
    }

def load_log(filepath):
    """
    读取并解析整个日志文件，返回一个 pandas.DataFrame。
    """
    records = []
    with open(filepath, 'r', encoding='utf-8') as f:
        for line in f:
            rec = parse_log_line(line)
            if rec:
                records.append(rec)
    df = pd.DataFrame.from_records(records)
    if df.empty:
        raise ValueError("没有解析到任何有效日志记录，请检查日志文件格式。")
    # 按时间排序
    df.sort_values(['timestamp', 'iface'], inplace=True)
    return df

def plot_loss_rates(df, output_file=None):
    """
    为每个接口绘制 RX 和 TX 丢包率随时间的曲线图。
    如果指定 output_file，则保存为图片，否则直接显示。
    """
    interfaces = df['iface'].unique()
    plt.figure(figsize=(12, 6))
    for iface in interfaces:
        sub = df[df['iface'] == iface]
        plt.plot(sub['timestamp'], sub['rx'], label=f'{iface} RX')
        plt.plot(sub['timestamp'], sub['tx'], label=f'{iface} TX', linestyle='--')
    plt.xlabel('Time')
    plt.ylabel('Loss Rate')
    plt.title('Interface Packet Loss Rate Over Time')
    plt.legend()
    plt.grid(True)
    plt.tight_layout()
    if output_file:
        plt.savefig(output_file)
        print(f'已保存图像到 {output_file}')
    else:
        plt.show()

def main():
    import argparse

    parser = argparse.ArgumentParser(description="解析 net_loss_rate_ip.log 并绘制丢包率曲线")
    parser.add_argument('logfile', type=Path, help="日志文件路径")
    parser.add_argument('-o', '--output', type=Path,
                        help="输出图像文件（可选），例如 loss_rate.png")
    args = parser.parse_args()

    if not args.logfile.exists():
        print(f"错误：找不到文件 {args.logfile}", file=sys.stderr)
        sys.exit(1)

    df = load_log(str(args.logfile))
    plot_loss_rates(df, output_file=str(args.output) if args.output else None)

if __name__ == '__main__':
    main()
