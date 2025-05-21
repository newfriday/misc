#!/usr/bin/env bash
#
# 使用 ip -s link show 来计算每秒丢包率，并记录到日志
# 用法： ./net_loss_rate_ip.sh eth0 wlan0 ...

# 日志文件
LOG_FILE="./net_loss_rate_ip.log"

# 处理 SIGINT/SIGTERM，优雅退出
cleanup() {
    echo "终止脚本，退出..." >&2
    exit 0
}
trap cleanup SIGINT SIGTERM

# 检查参数
if [ $# -lt 1 ]; then
    echo "用法: $0 <interface1> [interface2 ...]" >&2
    exit 1
fi
interfaces=("$@")

# 关联数组存储上一轮的统计值
declare -A prev_rx_pkts prev_rx_drop prev_tx_pkts prev_tx_drop

# 读取一次初始值
for iface in "${interfaces[@]}"; do
    # 使用 ip -s link show 提取：RX_packets, RX_dropped, TX_packets, TX_dropped
    read -r rx_pkts rx_drop tx_pkts tx_drop < <(
        ip -s link show dev "$iface" \
        | awk '
            $1=="RX:"     { getline; print $2, $4; }
            $1=="TX:"     { getline; print $2, $4; }
        ' \
        | xargs -n4 echo    # 把两行合并为一行：<rx_pkts> <rx_drop> <tx_pkts> <tx_drop>
    )
    prev_rx_pkts["$iface"]=$rx_pkts
    prev_rx_drop["$iface"]=$rx_drop
    prev_tx_pkts["$iface"]=$tx_pkts
    prev_tx_drop["$iface"]=$tx_drop
done

echo "开始监控接口：${interfaces[*]}, 日志: $LOG_FILE"
echo "时间戳 接口 RX_loss_rate TX_loss_rate" > "$LOG_FILE"

# 每秒循环
while true; do
    sleep 1
    timestamp=$(date '+%F %T')
    for iface in "${interfaces[@]}"; do
        # 再次采样
        read -r cur_rx_pkts cur_rx_drop cur_tx_pkts cur_tx_drop < <(
            ip -s link show dev "$iface" \
            | awk '
                $1=="RX:"     { getline; print $2, $4; }
                $1=="TX:"     { getline; print $2, $4; }
            ' \
            | xargs -n4 echo
        )

        # 计算增量
        delta_rx_pkts=$((cur_rx_pkts - prev_rx_pkts["$iface"]))
        delta_rx_drop=$((cur_rx_drop - prev_rx_drop["$iface"]))
        delta_tx_pkts=$((cur_tx_pkts - prev_tx_pkts["$iface"]))
        delta_tx_drop=$((cur_tx_drop - prev_tx_drop["$iface"]))

        # 计算丢包率
        if (( delta_rx_pkts > 0 )); then
            rx_loss_rate=$(awk "BEGIN{ printf \"%.6f\", $delta_rx_drop/$delta_rx_pkts }")
        else
            rx_loss_rate="0"
        fi
        if (( delta_tx_pkts > 0 )); then
            tx_loss_rate=$(awk "BEGIN{ printf \"%.6f\", $delta_tx_drop/$delta_tx_pkts }")
        else
            tx_loss_rate="0"
        fi

        # 记录日志
        printf "%s %s RX_loss_rate=%s TX_loss_rate=%s\n" \
            "$timestamp" "$iface" "$rx_loss_rate" "$tx_loss_rate" \
            >> "$LOG_FILE"

        # 更新上一轮数据
        prev_rx_pkts["$iface"]=$cur_rx_pkts
        prev_rx_drop["$iface"]=$cur_rx_drop
        prev_tx_pkts["$iface"]=$cur_tx_pkts
        prev_tx_drop["$iface"]=$cur_tx_drop
    done
done
