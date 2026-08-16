import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import seaborn as sns
import os

os.makedirs('../plots', exist_ok=True)
ORDER = ['fork', 'thread', 'epoll']

try:
    df = pd.read_csv('../logs/delivery_times.csv', header=None,
                     names=['server_type', 'rtt_ms'])
    plt.figure(figsize=(8, 6))
    # Log scale: the distribution is heavily skewed, and on a linear axis the
    # long tail flattens the box plots into indistinguishable lines.
    sns.boxplot(data=df, x='server_type', y='rtt_ms', order=ORDER, showfliers=False)
    plt.yscale('log')
    plt.title('Message Delivery Time Distribution (outliers hidden)')
    plt.ylabel('Round Trip Time (ms, log scale)')
    plt.xlabel('Server Architecture')
    plt.grid(True, axis='y', alpha=.3)
    plt.savefig('../plots/delivery_time_distribution.png', dpi=120, bbox_inches='tight')
    plt.close()
    print("Generated delivery_time_distribution.png")
except Exception as e:
    print(f"Skipping delivery times plot: {e}")

try:
    df = pd.read_csv('../logs/metrics.csv')

    metrics = {
        'cpu':    ('CPU Usage (%)',                     'cpu_usage.png',    False),
        'vmrss':  ('Resident Memory (VmRSS, KB)',       'vmrss_usage.png',  False),
        'pss':    ('Proportional Set Size (PSS, KB)',   'pss_usage.png',    False),
        'vmsize': ('Virtual Address Space (VmSize, KB)', 'vmsize_usage.png', True),
    }

    for col, (ylabel, filename, log_scale) in metrics.items():
        plt.figure(figsize=(10, 6))
        # Several reps per point, so seaborn draws the mean with a CI band.
        sns.lineplot(data=df, x='num_clients', y=col, hue='server_type',
                     hue_order=ORDER, marker='o', errorbar=('pi', 100))
        if log_scale:
            plt.yscale('log')
            ylabel += ' — log scale'
        plt.title(f'{ylabel} vs Concurrent Clients')
        plt.ylabel(ylabel)
        plt.xlabel('Number of Concurrent Clients')
        plt.grid(True, alpha=.3)
        plt.savefig(f'../plots/{filename}', dpi=120, bbox_inches='tight')
        plt.close()
        print(f"Generated {filename}")

except Exception as e:
    print(f"Skipping resource metrics plots: {e}")
