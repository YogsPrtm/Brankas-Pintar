<?php

use Illuminate\Database\Migrations\Migration;
use Illuminate\Database\Schema\Blueprint;
use Illuminate\Support\Facades\Schema;

return new class extends Migration
{
    /**
     * Run the migrations.
     */
    public function up(): void
    {
        Schema::create('perangkats', function (Blueprint $table) {
            $table->id('id_perangkat');
            $table->string('nama_brankas')->default('Brankas Utama');
            $table->enum('status_pintu', ['terkunci', 'terbuka'])->default('terkunci');
            $table->enum('status_alarm', ['aman','bahaya'])->default('aman');
            $table->timestamp('last_seen')->nullable();
            $table->timestamps();
        });
    }

    /**
     * Reverse the migrations.
     */
    public function down(): void
    {
        Schema::dropIfExists('perangkats');
    }
};
